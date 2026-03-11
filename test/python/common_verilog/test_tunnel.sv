`include "driver/byte_stream/edr_named_pipe_client.sv"
`include "driver/execution_gate/edr_execution_gate.sv"
`include "tunnel/edr_byte_stream_tunnel.sv"
`include "edr_context.sv"

module test_tunnel (
    output reg clk_o,

    output chandle context_handle_o,
    output event   context_initialized_event_o,

    output chandle execution_gate_handle_o,
    output event   execution_gate_initialized_event_o,

    input chandle driver_base_handle_i,
    input event   driver_initialized_event_i
);

  chandle pipe_handle;
  event   pipe_initialized_event;

  chandle execution_gate_base_handle;

  function static string get_pipe_name();
    string pipe_name;
    $value$plusargs("pipe=%s", pipe_name);
    return pipe_name;
  endfunction

  edr_context edr_context_instance (
      .log_level(EDR_LOG_LEVEL_TRACE),
      .log_to_std_streams(1),

      .context_handle_o(context_handle_o),
      .context_initialized_event_o(context_initialized_event_o)
  );

  edr_named_pipe_client edr_named_pipe_client_instance (
      .pipe_name_i(get_pipe_name()),

      .context_handle_i(context_handle_o),
      .context_initialized_event_i(context_initialized_event_o),

      .byte_stream_handle_o(pipe_handle),
      .byte_stream_initialized_event_o(pipe_initialized_event)
  );

  edr_execution_gate edr_execution_gate_instance (
      .context_handle_i(context_handle_o),
      .context_initialized_event_i(context_initialized_event_o),

      .execution_gate_handle_o(execution_gate_handle_o),
      .driver_base_handle_o(execution_gate_base_handle),
      .execution_gate_initialized_event_o(execution_gate_initialized_event_o)
  );

  edr_byte_stream_tunnel #(
      .NUM_DRIVERS(2)
  ) edr_byte_stream_tunnel_instance (
      .clk_i(clk_o),

      .context_handle_i(context_handle_o),
      .context_initialized_event_i(context_initialized_event_o),

      .byte_stream_handle_i(pipe_handle),
      .byte_stream_initialized_event_i(pipe_initialized_event),

      .driver_handles_i({execution_gate_base_handle, driver_base_handle_i}),
      .driver_initialized_events_i({execution_gate_initialized_event_o, driver_initialized_event_i})
  );

  initial begin
    $dumpfile("waves.vcd");
    $dumpvars(0);

    clk_o = 0;

    while (1) begin
      clk_o = 1;
      #10;
      clk_o = 0;
      #10;
    end
  end

endmodule
