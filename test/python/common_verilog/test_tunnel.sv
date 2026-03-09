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

    input chandle driver_handle_i,
    input event   driver_initialized_event_i,

    input system_is_idle_i
);

  chandle pipe_handle;
  event   pipe_initialized_event;

  function static string get_pipe_name();
    string pipe_name;
    $value$plusargs("pipe=%s", pipe_name);
    return pipe_name;
  endfunction

  edr_context edr_context_instance (
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
      .clk_i(clk_o),
      .system_is_idle_i(system_is_idle_i),

      .context_handle_i(context_handle_o),
      .context_initialized_event_i(context_initialized_event_o),

      .execution_gate_handle_o(execution_gate_handle_o),
      .execution_gate_initialized_event_o(execution_gate_initialized_event_o)
  );

  edr_byte_stream_tunnel edr_byte_stream_tunnel_instance (
      .clk_i(clk_o),

      .context_handle_i(context_handle_o),
      .context_initialized_event_i(context_initialized_event_o),

      .byte_stream_handle_i(pipe_handle),
      .byte_stream_initialized_event_i(pipe_initialized_event),

      .driver_handles_i({driver_handle_i}),
      .driver_initialized_events_i({driver_initialized_event_i})
  );

  initial begin
    clk_o = 0;

    while (1) begin
      clk_o = 1;
      #10;
      clk_o = 0;
      #10;
    end
  end

endmodule
