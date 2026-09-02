`timescale 1ns / 1ps

module spi_image_top (
    input  wire clk,

    input  wire spi_sclk,
    input  wire spi_mosi,
    input  wire spi_cs_n,
    output wire spi_miso,

    output reg  led_done,
    output reg  led_error,

    // 32-bit BRAM write interface
    // Connect internally to Block Memory Generator Port A.
    // Do NOT make these external FPGA pins.
    output reg  [31:0] bram_addr,   // 2048 words for 8192 pixels
    output reg  [31:0] bram_din,
    output reg  [3:0]  bram_we,

    // Status signals for later AXI GPIO/status logic.
    // Do NOT make these external FPGA pins.
    output reg         image_ready,
    output reg  [15:0] image_width,
    output reg  [15:0] image_height
);

    // ------------------------------------------------------------
    // Power-on reset
    // ------------------------------------------------------------
    reg [15:0] reset_count = 16'd0;
    reg rst_l = 1'b0;

    always @(posedge clk) begin
        if (reset_count != 16'hFFFF) begin
            reset_count <= reset_count + 16'd1;
            rst_l <= 1'b0;
        end else begin
            rst_l <= 1'b1;
        end
    end

    // ------------------------------------------------------------
    // SPI byte wires
    // ------------------------------------------------------------
    wire       rx_dv;
    wire [7:0] rx_byte;

    reg        tx_dv;
    reg [7:0]  tx_byte;

    SPI_Byte #(
        .SPI_MODE(0)
    ) spi_byte_inst (
        .i_Rst_L     (rst_l),
        .i_Clk       (clk),

        .o_RX_DV     (rx_dv),
        .o_RX_Byte   (rx_byte),

        .i_TX_DV     (tx_dv),
        .i_TX_Byte   (tx_byte),

        .i_SPI_Clk   (spi_sclk),
        .o_SPI_MISO  (spi_miso),
        .i_SPI_MOSI  (spi_mosi),
        .i_SPI_CS_n  (spi_cs_n)
    );

    // ------------------------------------------------------------
    // Synchronize chip select into FPGA clock domain
    // ------------------------------------------------------------
    reg cs_1;
    reg cs_2;

    always @(posedge clk) begin
        cs_1 <= spi_cs_n;
        cs_2 <= cs_1;
    end

    wire cs_high = cs_2;

    // ------------------------------------------------------------
    // Frame receive states
    // ------------------------------------------------------------
    localparam S_WAIT_START = 4'd0;
    localparam S_FRAME_ID   = 4'd1;
    localparam S_WIDTH_H    = 4'd2;
    localparam S_WIDTH_L    = 4'd3;
    localparam S_HEIGHT_H   = 4'd4;
    localparam S_HEIGHT_L   = 4'd5;
    localparam S_PIXELS     = 4'd6;
    localparam S_SUM_H      = 4'd7;
    localparam S_SUM_L      = 4'd8;
    localparam S_DONE       = 4'd9;

    reg [3:0] state;

    reg [7:0]  frame_id;
    reg [15:0] width;
    reg [15:0] height;

    reg [31:0] expected_pixels;
    reg [31:0] pixel_count;

    reg [15:0] pixel_sum_calc;
    reg [15:0] pixel_sum_rx;

    reg [23:0] led_count;

    // ------------------------------------------------------------
    // Image memory size
    // ------------------------------------------------------------
    localparam MAX_PIXELS = 32768;

    // Counts pixels, not BRAM words.
    // Valid pixel indexes are 0 through 8191.
    reg [15:0] write_addr;

    // Packs four 8-bit pixels into one 32-bit BRAM word.
    reg [31:0] pack_word;
    reg [1:0]  pack_count;

    // ------------------------------------------------------------
    // Main frame checker + 32-bit BRAM loader
    // ------------------------------------------------------------
    always @(posedge clk) begin
        if (!rst_l) begin
            state           <= S_WAIT_START;
            frame_id        <= 8'd0;
            width           <= 16'd0;
            height          <= 16'd0;
            expected_pixels <= 32'd0;
            pixel_count     <= 32'd0;
            pixel_sum_calc  <= 16'd0;
            pixel_sum_rx    <= 16'd0;

            write_addr      <= 14'd0;
            pack_word       <= 32'd0;
            pack_count      <= 2'd0;

            tx_byte         <= 8'h00;
            tx_dv           <= 1'b1;

            led_done        <= 1'b0;
            led_error       <= 1'b0;
            led_count       <= 24'd0;

            bram_addr       <= 32'd0;
            bram_din        <= 32'd0;
            bram_we         <= 4'b0000;

            image_ready     <= 1'b0;
            image_width     <= 16'd0;
            image_height    <= 16'd0;
        end else begin
            tx_dv   <= 1'b0;
            bram_we <= 4'b0000;

            // If CS is high, get ready for a new frame
            if (cs_high) begin
                state           <= S_WAIT_START;
                pixel_count     <= 32'd0;
                pixel_sum_calc  <= 16'd0;
                pixel_sum_rx    <= 16'd0;
                write_addr      <= 14'd0;
                pack_word       <= 32'd0;
                pack_count      <= 2'd0;
                tx_byte         <= 8'h00;
                tx_dv           <= 1'b1;
            end

            if (rx_dv) begin
                case (state)

                    // ------------------------------------------------
                    // Wait for start byte 0xA5
                    // ------------------------------------------------
                    S_WAIT_START: begin
                        led_done       <= 1'b0;
                        led_error      <= 1'b0;
                        image_ready    <= 1'b0;

                        pixel_count    <= 32'd0;
                        pixel_sum_calc <= 16'd0;
                        pixel_sum_rx   <= 16'd0;
                        write_addr     <= 14'd0;
                        pack_word      <= 32'd0;
                        pack_count     <= 2'd0;

                        bram_addr      <= 32'd0;
                        bram_din       <= 32'd0;
                        bram_we        <= 4'b0000;

                        if (rx_byte == 8'hA5) begin
                            state   <= S_FRAME_ID;
                            tx_byte <= 8'h10;
                            tx_dv   <= 1'b1;
                        end else begin
                            state   <= S_WAIT_START;
                            tx_byte <= 8'hE0;
                            tx_dv   <= 1'b1;
                        end
                    end

                    // ------------------------------------------------
                    // Read frame ID
                    // ------------------------------------------------
                    S_FRAME_ID: begin
                        frame_id <= rx_byte;
                        state    <= S_WIDTH_H;
                        tx_byte  <= 8'h11;
                        tx_dv    <= 1'b1;
                    end

                    // ------------------------------------------------
                    // Read width
                    // ------------------------------------------------
                    S_WIDTH_H: begin
                        width[15:8] <= rx_byte;
                        state       <= S_WIDTH_L;
                        tx_byte     <= 8'h12;
                        tx_dv       <= 1'b1;
                    end

                    S_WIDTH_L: begin
                        width[7:0]  <= rx_byte;
                        image_width <= {width[15:8], rx_byte};

                        state       <= S_HEIGHT_H;
                        tx_byte     <= 8'h13;
                        tx_dv       <= 1'b1;
                    end

                    // ------------------------------------------------
                    // Read height
                    // ------------------------------------------------
                    S_HEIGHT_H: begin
                        height[15:8] <= rx_byte;
                        state        <= S_HEIGHT_L;
                        tx_byte      <= 8'h14;
                        tx_dv        <= 1'b1;
                    end

                    S_HEIGHT_L: begin
                        height[7:0]  <= rx_byte;
                        image_height <= {height[15:8], rx_byte};

                        expected_pixels <= width * {height[15:8], rx_byte};

                        pixel_count     <= 32'd0;
                        pixel_sum_calc  <= 16'd0;
                        write_addr      <= 14'd0;
                        pack_word       <= 32'd0;
                        pack_count      <= 2'd0;

                        if ((width * {height[15:8], rx_byte}) > MAX_PIXELS) begin
                            led_done  <= 1'b0;
                            led_error <= 1'b1;

                            tx_byte <= 8'hEF;   // too many pixels
                            tx_dv   <= 1'b1;

                            state <= S_DONE;
                        end else begin
                            state   <= S_PIXELS;
                            tx_byte <= 8'h15;
                            tx_dv   <= 1'b1;
                        end
                    end

                    // ------------------------------------------------
                    // Receive pixels, pack 4 bytes into one 32-bit BRAM word,
                    // and compute 16-bit sum
                    // ------------------------------------------------
                    S_PIXELS: begin
                        if (write_addr < MAX_PIXELS) begin

                            // Pack pixels little-endian:
                            // word[7:0]   = pixel 0
                            // word[15:8]  = pixel 1
                            // word[23:16] = pixel 2
                            // word[31:24] = pixel 3
                            case (pack_count)
                                2'd0: begin
                                    pack_word[7:0] <= rx_byte;

                                    // If this is also the last pixel, write partial word.
                                    if (pixel_count + 32'd1 == expected_pixels) begin
                                        bram_addr <= {17'd0, write_addr[15:2], 2'b00};
                                        bram_din  <= {24'd0, rx_byte};
                                        bram_we   <= 4'b0001;
                                    end
                                end

                                2'd1: begin
                                    pack_word[15:8] <= rx_byte;

                                    if (pixel_count + 32'd1 == expected_pixels) begin
                                        bram_addr <= {17'd0, write_addr[15:2], 2'b00};
                                        bram_din  <= {16'd0, rx_byte, pack_word[7:0]};
                                        bram_we   <= 4'b0011;
                                    end
                                end

                                2'd2: begin
                                    pack_word[23:16] <= rx_byte;

                                    if (pixel_count + 32'd1 == expected_pixels) begin
                                        bram_addr <= {17'd0, write_addr[15:2], 2'b00};
                                        bram_din  <= {8'd0, rx_byte, pack_word[15:0]};
                                        bram_we   <= 4'b0111;
                                    end
                                end

                                2'd3: begin
                                    pack_word[31:24] <= rx_byte;

                                    // Full 32-bit word ready.
                                    bram_addr <= {17'd0, write_addr[15:2], 2'b00};
                                    bram_din  <= {rx_byte, pack_word[23:0]};
                                    bram_we   <= 4'b1111;
                                end
                            endcase

                            if (pack_count == 2'd3) begin
                                pack_count <= 2'd0;
                                pack_word  <= 32'd0;
                            end else begin
                                pack_count <= pack_count + 2'd1;
                            end

                            write_addr <= write_addr + 14'd1;

                            pixel_sum_calc <= pixel_sum_calc + rx_byte;
                            pixel_count    <= pixel_count + 32'd1;

                            if (pixel_count + 32'd1 == expected_pixels) begin
                                state <= S_SUM_H;
                            end

                            tx_byte <= 8'h20;
                            tx_dv   <= 1'b1;
                        end else begin
                            led_done  <= 1'b0;
                            led_error <= 1'b1;

                            tx_byte <= 8'hEE;
                            tx_dv   <= 1'b1;

                            state <= S_DONE;
                        end
                    end

                    // ------------------------------------------------
                    // Receive expected sum high byte
                    // ------------------------------------------------
                    S_SUM_H: begin
                        pixel_sum_rx[15:8] <= rx_byte;
                        state   <= S_SUM_L;
                        tx_byte <= 8'h21;
                        tx_dv   <= 1'b1;
                    end

                    // ------------------------------------------------
                    // Receive expected sum low byte and compare
                    // ------------------------------------------------
                    S_SUM_L: begin
                        pixel_sum_rx[7:0] <= rx_byte;

                        if ({pixel_sum_rx[15:8], rx_byte} == pixel_sum_calc) begin
                            led_done    <= 1'b1;
                            led_error   <= 1'b0;
                            led_count   <= 24'd10_000_000;
                            image_ready <= 1'b1;

                            tx_byte <= 8'hAA;   // PASS
                            tx_dv   <= 1'b1;
                        end else begin
                            led_done    <= 1'b0;
                            led_error   <= 1'b1;
                            image_ready <= 1'b0;

                            tx_byte <= 8'hEE;   // FAIL
                            tx_dv   <= 1'b1;
                        end

                        state <= S_DONE;
                    end

                    // ------------------------------------------------
                    // Done state
                    // ------------------------------------------------
                    S_DONE: begin
                        state   <= S_WAIT_START;
                        tx_byte <= 8'h00;
                        tx_dv   <= 1'b1;
                    end

                    default: begin
                        state <= S_WAIT_START;
                    end

                endcase
            end

            // Green LED visible pulse
            if (led_count != 24'd0) begin
                led_count <= led_count - 24'd1;
            end else begin
                // led_done <= 1'b0;
            end
        end
    end

endmodule