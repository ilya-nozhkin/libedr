module edr_byte_stream_tunnel #(
    parameter int unsigned NUM_DRIVERS = 1
) (
    input clk_i,

    input chandle context_handle_i,
    input event   context_initialized_event_i,

    input chandle byte_stream_handle_i,
    input event   byte_stream_initialized_event_i,

    input chandle driver_handles_i[NUM_DRIVERS],
    input event driver_initialized_events_i[NUM_DRIVERS]
);
  chandle tunnel_handle;
  chandle error_handle;

  import "DPI-C" function chandle edr_Error_new();
  import "DPI-C" function void edr_Error_delete(chandle error);
  import "DPI-C" function string edr_Error_Message(chandle error);
  import "DPI-C" function byte edr_Error_Fail(chandle error);

  import "DPI-C" function chandle edr_ByteStreamTunnel_new(
    chandle ctx,
    chandle byte_stream
  );

  import "DPI-C" function void edr_ByteStreamTunnel_RegisterDriver(
    chandle tunnel,
    chandle driver
  );

  import "DPI-C" function void edr_ByteStreamTunnel_StartServer(
    chandle tunnel,
    chandle error
  );

  import "DPI-C" function byte edr_ByteStreamTunnel_IsAlive(chandle tunnel);

  import "DPI-C" function void edr_ByteStreamTunnel_delete(chandle tunnel);

  initial begin
    error_handle  = edr_Error_new();
    tunnel_handle = 0;

    wait (context_initialized_event_i.triggered);
    wait (byte_stream_initialized_event_i.triggered);

    tunnel_handle = edr_ByteStreamTunnel_new(context_handle_i, byte_stream_handle_i);

    for (int unsigned i = 0; i < NUM_DRIVERS; i++) begin
      wait (driver_initialized_events_i[i].triggered);
      edr_ByteStreamTunnel_RegisterDriver(tunnel_handle, driver_handles_i[i]);
    end

    edr_ByteStreamTunnel_StartServer(tunnel_handle, error_handle);
    if (0 != edr_Error_Fail(error_handle)) begin
      $display("Failed to start serving the byte stream tunnel: ", edr_Error_Message(error_handle));
      $finish(0);
    end
  end

  final begin
    if (tunnel_handle != 0) begin
      edr_ByteStreamTunnel_delete(tunnel_handle);
    end
  end

  always @(negedge clk_i) begin
    if (tunnel_handle != 0) begin
      if (0 == edr_ByteStreamTunnel_IsAlive(tunnel_handle)) begin
        $finish(0);
      end
    end
  end

endmodule
