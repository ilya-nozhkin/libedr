module test_tunnel (
    output reg clk_o,
    output reg resetn_o,

    output edr_Error error_o,
    output edr_Context context_o,
    output edr_ExecutionGate execution_gate_o,
    output edr_ByteStreamTunnel tunnel_o
);
  edr_ByteStream pipe;

  function static string get_pipe_name();
    string pipe_name;
    $value$plusargs("pipe=%s", pipe_name);
    return pipe_name;
  endfunction

  initial begin
    $dumpfile("waves.vcd");
    $dumpvars(0);

    error_o = make_edr_Error();

    context_o = make_edr_Context(edr_LogLevel_TRACE);
    execution_gate_o = make_edr_ExecutionGate(context_o, "ExecutionGate");

    pipe = make_edr_ByteStream_ConnectNamedPipe(context_o, get_pipe_name(), error_o);
    if (error_o.Fail()) begin
      $display("Failed to connect to named pipe '%s': %s", get_pipe_name(), error_o.Message());
      $finish(1);
    end

    tunnel_o = make_edr_ByteStreamTunnel(context_o, pipe);
    tunnel_o.RegisterDriver(execution_gate_o);
  end

  initial begin
    clk_o = 0;

    while (1) begin
      clk_o = 1;
      #10;
      clk_o = 0;
      #10;
    end
  end

  initial begin
    resetn_o = 0;
    #50 resetn_o = 1;
  end

endmodule
