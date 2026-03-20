module edr_tcp_server #(
    parameter shortint unsigned PORT = 3997
) (
    input chandle context_handle_i,

    output chandle byte_stream_handle_o
);
  chandle error_handle;
  chandle tcp_server_handle;

  import "DPI-C" function chandle edr_Error_new();
  import "DPI-C" function void edr_Error_delete(chandle error);
  import "DPI-C" function string edr_Error_Message(chandle error);
  import "DPI-C" function byte edr_Error_Fail(chandle error);

  import "DPI-C" function chandle edr_TCPServer_new(
    input chandle ctx,
    input shortint unsigned port,
    input int max_num_queued_clients,
    input chandle error
  );

  import "DPI-C" function chandle edr_TCPServer_Accept(
    input chandle tcp_server,
    input chandle error
  );

  import "DPI-C" function void edr_TCPServer_delete(input chandle tcp_server);

  import "DPI-C" function void edr_ByteStream_delete(input chandle byte_stream);

  initial begin
    error_handle = edr_Error_new();
    tcp_server_handle = 0;
    byte_stream_handle_o = 0;

    wait (context_handle_i != 0);

    tcp_server_handle = edr_TCPServer_new(context_handle_i, PORT, 1, error_handle);
    if (0 != edr_Error_Fail(error_handle)) begin
      $display("Failed to create a TCP server: ", edr_Error_Message(error_handle));
      $finish(0);
    end else begin
      byte_stream_handle_o = edr_TCPServer_Accept(tcp_server_handle, error_handle);
      if (0 != edr_Error_Fail(error_handle)) begin
        $display("Failed to create a TCP server: ", edr_Error_Message(error_handle));
        $finish(0);
      end
    end
  end

  final begin
    if (tcp_server_handle != 0) begin
      edr_TCPServer_delete(tcp_server_handle);
    end

    if (byte_stream_handle_o != 0) begin
      edr_ByteStream_delete(byte_stream_handle_o);
    end

    edr_Error_delete(error_handle);
  end
endmodule
