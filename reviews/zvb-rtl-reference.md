# ZVB RTL reference evidence

Read-only excerpts from the existing version 1.0.0 RTL checkout. These establish
source behavior; they are not physical-board test results.

## ZealLogic.v

SHA256 `3c55523da34f714155ff3172dbdfc43234ce03e7365a6abe7cd693a318b2978c`

```verilog
118:     endfunction
119:
120:     /* Only keep the highest nibble, [15:8]. Since the upper 64KB is allocated to the tileset, include
121:      * bit 16 in the comparison, make sure it is 0. */
122:     /* Layer0 is mapped at [0000;0C80] */
123:     assign tilemap0_mem = inRange(address[16:4], 'h0, 'hc8)    & fpga_core_mem_ce & data_sig_valid;
124:     /* Palette is mapped at [0E00;1000] */
125:     assign palette_mem  = inRange(address[16:4], 'he0, 'h100)  & fpga_core_mem_ce & data_sig_valid;
126:     /* Layer1 is mapped at [1000;1C80] */
127:     assign tilemap1_mem = inRange(address[16:4], 'h100, 'h1c8) & fpga_core_mem_ce & data_sig_valid;
224:                 /* Scratch registers */
225:                 4'h8: scratch[0] <= data;
226:                 4'h9: scratch[1] <= data;
227:                 4'ha: scratch[2] <= data;
228:                 4'hb: scratch[3] <= data;
229:                 /* Change the device mapped in [A0;AF] on the I/O bus */
230:                 4'he: mapped_device <= data[5:0];
231:                 /* Change the physical address space of the FPGA 128KB memory */
232:                 4'hf: fpga_phys_addr <= data[4:0];
233:                 default:;
234:             endcase
235:             /* Make sure the RPU is not writing when we read! */
236:         end else if (fpga_mapping_io & ~rd_n & ~rpu_wr) begin
237:             /* Return the configuration registers */
238:             case (address[3:0])
239:                 4'h0: data_out <= `REV;
240:                 4'h1: data_out <= `MINOR;
241:                 4'h2: data_out <= `MAJOR;
242:                 4'h8: data_out <= scratch[0];
243:                 4'h9: data_out <= scratch[1];
244:                 4'ha: data_out <= scratch[2];
245:                 4'hb: data_out <= scratch[3];
246:                 4'he: data_out <= { 2'b0, mapped_device  };
247:                 4'hf: data_out <= { 3'b0, fpga_phys_addr };
248:                 default:;
```

## ZealPalette.v

SHA256 `98f4cd1300550478353a6f88f888ae4dc41e8ce97b2c15e6364202f128ad6c87`

```verilog
18:     (* no_rw_check *)
19:     reg [15:0] ram [0:255];
20:
21:     /* Write to the LSB on even addresses */
22:     wire lsb = ~address_bus[0];
23:     wire [7:0] ext_bus_addr = address_bus[8:1];
24:     /* Latches for the writes and the reads */
25:     reg [7:0] latched_value;
26:     /* Latch for the reads only */
27:     reg [15:0] latched_value_rd;
28:     /* Only output the relevant byte to the outside */
29:     assign data_out = lsb ? latched_value_rd[7:0] : latched_value_rd[15:8];
30:
31:
32:     initial begin
33:         $readmemh("default_palette.mem", ram);
34:     end
35:
36:     /* Port A */
37:     always @ (posedge clock) begin
38:         q_a <= ram[read_address];
39:     end
40:
41:     /* Port B */
42:     always @ (posedge clock) begin
43:         if (~wr_n & ce) begin
44:             if (lsb)
45:                 latched_value <= data;
46:             else
47:                 ram[ext_bus_addr] <= { data, latched_value };
48:         end else if (~rd_n & ce) begin
49:             latched_value_rd <= ram[ext_bus_addr];
50:         end
51:     end
52:
53: endmodule
54:
```

## zeal.v

SHA256 `52bd19cfa8ec0037aa5ab92d4505ec5135a335e6a05864f3f5b54936c0f12039`

```verilog
99:         else if (~core_read_req & spi_ce)       data_bus_output <= data_out_spi;
100:         else if (~core_read_req & vconfig_ce)   data_bus_output <= data_out_vconfig;
101:         else if (~core_read_req & crc32_ce)     data_bus_output <= data_out_crc32;
102:         else if (~core_read_req & charset_mem)  data_bus_output <= data_out_charset;
103:         else if (~core_read_req & rpu_ce)       data_bus_output <= data_out_rpu;
104: `ifdef TIMERMODULE
105:         else if (~core_read_req & timer_ce)     data_bus_output <= data_out_timer;
106: `endif
107:         else if (~core_read_req & dma_ce)       data_bus_output <= data_out_dma;
108:         else if (~core_read_req & tilemap_io)   data_bus_output <= data_out_tilemap_io;
109:         else if (~core_read_req & sprites_mem)  data_bus_output <= data_out_sprites;
110:         else if (~core_read_req & palette_mem)  data_bus_output <= data_out_palette;
111:         // else if (math_ce)      data_bus_output <= data_out_math;
141:         assign data_bus_out = dma_active ? data_bus_dma : data_bus_output;
142:
143: `else // !VERILATOR_RUNTIME
144:         /* The address shall be controlled by the FPGA when a DMA transaction is on-going */
145:         assign address_bus = dma_active ? address_bus_dma : 22'bz;
146:         assign io_req      = dma_active ? io_req_dma      : 1'bz;
147:         assign memory_req  = dma_active ? memory_req_dma  : 1'bz;
148:         assign write_req   = dma_active ? write_req_dma   : 1'bz;
149:         assign read_req    = dma_active ? read_req_dma    : 1'bz;
611:     ZealPalette palette_inst(.clock(clk),
612: 	                         .read_address(palette_input_addr),
613:                              .address_bus(core_addr_bus[15:0]),
614: 	                         .data(core_data_bus),
615:                              .ce(palette_mem),
616: 	                         .wr_n(core_write_req),
617:                              .rd_n(core_read_req),
618:                              .data_out(data_out_palette),
619: 	                         .q_a(palette_output));
620:
621:     /**
```

## 4bpp layer1 attributes in zeal.v

Same zeal.v SHA256 as above. These are source-derived fields, not a board test.

```verilog
713:     wire [3:0]  tile_column_layer0_4bit = tilemap_output_layer1[3] ? ~layer0_render_x[3:0] : layer0_render_x[3:0];
714:     wire [3:0]  tile_line_layer0_4bit   = tilemap_output_layer1[2] ? ~layer0_render_y[3:0] : layer0_render_y[3:0];
811:     wire        tileset_index = pixel_clock ? tilemap_output_layer1[0]   : sprite_tileset_index;
812:     wire [3:0]  tileset_palet = pixel_clock ? tilemap_output_layer1[7:4] : sprite_tileset_palette;
```
