module edr_named_pipe_client (
    input string pipe_name_i,

    input chandle context_handle_i,

    output chandle byte_stream_handle_o
);
  chandle error_handle;

  import "DPI-C" function chandle edr_Error_new();
  import "DPI-C" function void edr_Error_delete(chandle error);
  import "DPI-C" function string edr_Error_Message(chandle error);
  import "DPI-C" function byte edr_Error_Fail(chandle error);

  import "DPI-C" function chandle edr_ByteStream_ConnectNamedPipe(
    input chandle ctx,
    input string  name,
    input chandle error
  );

  import "DPI-C" function void edr_ByteStream_delete(input chandle byte_stream);

  initial begin
    error_handle = edr_Error_new();
    byte_stream_handle_o = 0;

    wait (context_handle_i != 0);

    byte_stream_handle_o =
        edr_ByteStream_ConnectNamedPipe(context_handle_i, pipe_name_i, error_handle);
    if (0 != edr_Error_Fail(error_handle)) begin
      $display("Failed to connect to named pipe '%s': %s", pipe_name_i, edr_Error_Message(
               error_handle));
      $finish(0);
    end
  end

  final begin
    if (byte_stream_handle_o != 0) begin
      edr_ByteStream_delete(byte_stream_handle_o);
    end

    edr_Error_delete(error_handle);
  end
endmodule
