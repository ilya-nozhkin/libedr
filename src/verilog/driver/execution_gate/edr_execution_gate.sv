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
    input chandle context_handle_i,
    input event   context_initialized_event_i,

    output chandle execution_gate_handle_o,
    output chandle driver_base_handle_o,
    output event   execution_gate_initialized_event_o
);
  import "DPI-C" function chandle edr_ExecutionGate_new(
    input chandle ctx,
    input string  name
  );

  import "DPI-C" function void edr_ExecutionGate_delete(input chandle exe_gate);

  import "DPI-C" function int unsigned edr_ExecutionGate_SetMode(
    input chandle exe_gate,
    input int unsigned mode
  );

  import "DPI-C" function chandle edr_ExecutionGate_CastToBase(input chandle exe_gate);

  initial begin
    execution_gate_mode_t effective_mode;
    execution_gate_handle_o = 0;

    wait (context_initialized_event_i.triggered);

    execution_gate_handle_o = edr_ExecutionGate_new(context_handle_i, NAME);
    driver_base_handle_o = edr_ExecutionGate_CastToBase(execution_gate_handle_o);

    effective_mode = execution_gate_mode_t
        '(edr_ExecutionGate_SetMode(execution_gate_handle_o, int'(INITIAL_MODE)));
    if (effective_mode != INITIAL_MODE) begin
      $display("The effective execution gate mode is different from the requested one");
      $finish(0);
    end else begin
      ->execution_gate_initialized_event_o;
    end
  end

  final begin
    if (execution_gate_handle_o != 0) begin
      edr_ExecutionGate_delete(execution_gate_handle_o);
    end
  end
endmodule
