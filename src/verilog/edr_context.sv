typedef enum int unsigned {
  EDR_LOG_LEVEL_ERROR = 0,
  EDR_LOG_LEVEL_WARNING = 1,
  EDR_LOG_LEVEL_INFO = 2,
  EDR_LOG_LEVEL_DEBUG = 3,
  EDR_LOG_LEVEL_TRACE = 4
} edr_log_level_t;

module edr_context (
    input edr_log_level_t log_level,
    input log_to_std_streams,

    output chandle context_handle_o,
    output event   context_initialized_event_o
);
  import "DPI-C" function chandle edr_Context_new(input int unsigned log_level);
  import "DPI-C" function void edr_Context_AddStdStreams(input chandle ctx);
  import "DPI-C" function void edr_Context_delete(input chandle ctx);

  initial begin
    context_handle_o = edr_Context_new(int'(log_level));

    if (log_to_std_streams) begin
      edr_Context_AddStdStreams(context_handle_o);
    end

    ->context_initialized_event_o;
  end

  final begin
    edr_Context_delete(context_handle_o);
  end
endmodule
