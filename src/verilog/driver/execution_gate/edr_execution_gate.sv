typedef enum int unsigned {
  EDR_STALL_IF_NO_REQUESTS_AND_IDLE = 0,
  EDR_FORCE_STALL_IF_NO_REQUESTS = 1,
  EDR_FORCE_STALL = 2,
  EDR_NO_STALL = 3
} execution_gate_mode_t;

module edr_execution_gate #(
    parameter string NAME = "Execution Gate",
    parameter execution_gate_mode_t INITIAL_MODE = EDR_STALL_IF_NO_REQUESTS_AND_IDLE
) (
    input clk_i,
    input system_is_idle_i,

    input chandle context_handle_i,
    input event   context_initialized_event_i,

    output chandle execution_gate_handle_o,
    output event   execution_gate_initialized_event_o
);
  import "DPI-C" function chandle edr_ExecutionGate_new(
    input chandle ctx,
    input string  name
  );

  import "DPI-C" function void edr_ExecutionGate_delete(input chandle exe_gate);

  import "DPI-C" function int unsigned edr_ExecutionGate_SetMode(input int unsigned mode);

  import "DPI-C" function void edr_ExecutionGate_StallIfNeeded(input byte target_is_idle);

  initial begin
    execution_gate_mode_t effective_mode;
    execution_gate_handle_o = 0;

    wait (context_initialized_event_i.triggered);

    execution_gate_handle_o = edr_ExecutionGate_new(context_handle_i, NAME);
    effective_mode = execution_gate_mode_t'(edr_ExecutionGate_SetMode(int'(INITIAL_MODE)));

    ->execution_gate_initialized_event_o;
  end

  final begin
    if (execution_gate_handle_o != 0) begin
      edr_ExecutionGate_delete(execution_gate_handle_o);
    end
  end

  always @(negedge clk_i) begin
    if (execution_gate_handle_o != 0) begin
      edr_ExecutionGate_StallIfNeeded(system_is_idle_i ? byte'(1) : byte'(0));
    end
  end
endmodule
