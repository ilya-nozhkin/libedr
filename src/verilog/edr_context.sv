module edr_context (
    output chandle context_handle_o,
    output event   context_initialized_event_o
);
  import "DPI-C" function chandle edr_Context_new();
  import "DPI-C" function void edr_Context_delete(input chandle ctx);

  initial begin
    context_handle_o = edr_Context_new();
    ->context_initialized_event_o;
  end

  final begin
    edr_Context_delete(context_handle_o);
  end
endmodule
