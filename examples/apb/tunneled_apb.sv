`include "driver/byte_stream/edr_named_pipe_client.sv"
`include "driver/execution_gate/edr_execution_gate.sv"
`include "driver/apb/edr_apb.sv"
`include "tunnel/edr_byte_stream_tunnel.sv"
`include "edr_context.sv"

module tunneled_apb #(
    int ADDR_WIDTH = 32,
    int DATA_WIDTH = 32,
    int NUM_PSELS  = 1
) (
    input system_is_idle_i,

    input pclk,
    input presetn,

    output [ADDR_WIDTH-1:0] paddr,
    output [NUM_PSELS-1:0] psel,
    output penable,
    output pwrite,
    output [DATA_WIDTH-1:0] pwdata,

    input pready,
    input [DATA_WIDTH-1:0] prdata,
    input pslverr
);
  chandle context_handle;
  chandle execution_gate_handle;
  chandle pipe_handle;
  chandle apb_handle;

  function static string get_pipe_name();
    string pipe_name;
    $value$plusargs("edr-pipe=%s", pipe_name);
    return pipe_name;
  endfunction

  edr_context edr_context_instance (
      .log_level(EDR_LOG_LEVEL_TRACE),
      .log_to_std_streams(0),
      .log_to_file("edr_rtl.log"),

      .context_handle_o(context_handle)
  );

  edr_named_pipe_client edr_named_pipe_client_instance (
      .pipe_name_i(get_pipe_name()),

      .context_handle_i(context_handle),

      .byte_stream_handle_o(pipe_handle)
  );

  edr_execution_gate edr_execution_gate_instance (
      .context_handle_i(context_handle),

      .execution_gate_handle_o(execution_gate_handle)
  );

  edr_apb #(
      .ADDR_WIDTH(ADDR_WIDTH),
      .DATA_WIDTH(DATA_WIDTH),
      .TIMEOUT(100),
      .NAME("APB")
  ) edr_apb_instance (
      .system_is_idle_i(system_is_idle_i),

      .pclk(pclk),
      .presetn(presetn),

      .paddr(paddr),
      .psel(psel),
      .penable(penable),
      .pwrite(pwrite),
      .pwdata(pwdata),

      .pready (pready),
      .prdata (prdata),
      .pslverr(pslverr),

      .context_handle_i(context_handle),
      .execution_gate_handle_i(execution_gate_handle),
      .apb_handle_o(apb_handle)
  );

  edr_byte_stream_tunnel #(
      .NUM_DRIVERS(2)
  ) edr_byte_stream_tunnel_instance (
      .clk_i(pclk),

      .context_handle_i(context_handle),
      .byte_stream_handle_i(pipe_handle),
      .driver_handles_i({execution_gate_handle, apb_handle})
  );

endmodule
