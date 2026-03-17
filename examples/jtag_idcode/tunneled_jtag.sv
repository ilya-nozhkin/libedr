`include "driver/byte_stream/edr_named_pipe_client.sv"
`include "driver/execution_gate/edr_execution_gate.sv"
`include "driver/jtag/edr_jtag.sv"
`include "tunnel/edr_byte_stream_tunnel.sv"
`include "edr_context.sv"

module tunneled_jtag (
    input clk_i,
    input system_is_idle_i,

    output tck_o,
    output tms_o,
    output tdi_o,
    input  tdo_i
);
  chandle context_handle;
  event   context_initialized_event;

  chandle execution_gate_handle;
  chandle execution_gate_base_handle;
  event   execution_gate_initialized_event;

  chandle pipe_handle;
  event   pipe_initialized_event;

  chandle jtag_handle;
  chandle jtag_base_handle;
  event   jtag_initialized_event;

  function static string get_pipe_name();
    string pipe_name;
    $value$plusargs("edr-pipe=%s", pipe_name);
    return pipe_name;
  endfunction

  edr_context edr_context_instance (
      .log_level(EDR_LOG_LEVEL_TRACE),
      .log_to_std_streams(0),
      .log_to_file("edr_rtl.log"),

      .context_handle_o(context_handle),
      .context_initialized_event_o(context_initialized_event)
  );

  edr_named_pipe_client edr_named_pipe_client_instance (
      .pipe_name_i(get_pipe_name()),

      .context_handle_i(context_handle),
      .context_initialized_event_i(context_initialized_event),

      .byte_stream_handle_o(pipe_handle),
      .byte_stream_initialized_event_o(pipe_initialized_event)
  );

  edr_execution_gate edr_execution_gate_instance (
      .context_handle_i(context_handle),
      .context_initialized_event_i(context_initialized_event),

      .execution_gate_handle_o(execution_gate_handle),
      .driver_base_handle_o(execution_gate_base_handle),
      .execution_gate_initialized_event_o(execution_gate_initialized_event)
  );

  edr_jtag #(
      .BITS_PER_BATCH(16),
      .NAME("JTAG")
  ) edr_jtag_instance (
      .clk_i(clk_i),
      .system_is_idle_i(1),

      .tck_o(tck_o),
      .tms_o(tms_o),
      .tdi_o(tdi_o),
      .tdo_i(tdo_i),

      .context_handle_i(context_handle),
      .context_initialized_event_i(context_initialized_event),

      .execution_gate_handle_i(execution_gate_handle),
      .execution_gate_initialized_event_i(execution_gate_initialized_event),

      .jtag_handle_o(jtag_handle),
      .driver_base_handle_o(jtag_base_handle),
      .jtag_initialized_event_o(jtag_initialized_event)
  );

  edr_byte_stream_tunnel #(
      .NUM_DRIVERS(2)
  ) edr_byte_stream_tunnel_instance (
      .clk_i(clk_i),

      .context_handle_i(context_handle),
      .context_initialized_event_i(context_initialized_event),

      .byte_stream_handle_i(pipe_handle),
      .byte_stream_initialized_event_i(pipe_initialized_event),

      .driver_handles_i({execution_gate_base_handle, jtag_base_handle}),
      .driver_initialized_events_i({execution_gate_initialized_event, jtag_initialized_event})
  );

endmodule
