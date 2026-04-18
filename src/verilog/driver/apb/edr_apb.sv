module edr_apb #(
    parameter int ADDR_WIDTH = 32,
    parameter int DATA_WIDTH = 32,
    parameter int NUM_PSELS = 1,
    parameter int TIMEOUT = 100,
    parameter string NAME = "APB"
) (
    input system_is_idle_i,

    input pclk,
    input presetn,

    output [ADDR_WIDTH-1:0] paddr,
    output [NUM_PSELS-1:0] psel,
    output penable,
    output pwrite,
    output [DATA_WIDTH-1:0] pwdata,

    input pready,
    input [DATA_WIDTH-1:0] prdata,
    input pslverr,

    input edr_Context context_i,
    input edr_ExecutionGate execution_gate_i,
    output edr_APB apb_o
);
  parameter int COMMANDS_PER_BATCH = EDR_ARRAY_SIZE;

  edr_PullAPB pull_apb;

  typedef enum logic [6:0] {
    CMD_NOP = 0,
    CMD_SKIP_CYCLE = 1,
    CMD_WRITE = 2,
    CMD_READ = 3,
    CMD_SET_PSEL = 4
  } command_t;

  typedef enum byte {
    STATUS_OK = 0,
    STATUS_ERROR = 1,
    STATUS_TIMEOUT = 2
  } status_t;

  typedef enum byte {
    S_INVALID = 0,
    S_RESETTING = 1,
    S_OPERATING = 2,
    S_WAITING = 4,
    S_ERROR = 5,
    S_TIMEOUT = 6
  } state_t;

  state_t state;

  byte commands[COMMANDS_PER_BATCH];
  int unsigned input_addrs[COMMANDS_PER_BATCH];
  int unsigned input_data[COMMANDS_PER_BATCH];

  status_t captured_statuses[COMMANDS_PER_BATCH];
  int unsigned captured_output_data[COMMANDS_PER_BATCH];

  int unsigned num_commands;
  int unsigned current_command_index;
  int unsigned num_results;
  int unsigned psel_index;
  int unsigned cycles_left_until_timeout;

  logic [NUM_PSELS-1:0] effective_psel;
  assign effective_psel = {{NUM_PSELS - 1{1'b0}}, 1'b1} << psel_index;

  // [Input]

  logic command_valid = num_commands != 0;

  command_t current_command;
  assign current_command = command_valid ?
    command_t'(commands[current_command_index][6:0]) :
    CMD_NOP;

  logic beginning_of_xact;
  assign beginning_of_xact = command_valid & commands[current_command_index][7];

  wire [ADDR_WIDTH-1:0] current_addr;
  assign current_addr = input_addrs[current_command_index][ADDR_WIDTH-1:0];

  wire [DATA_WIDTH-1:0] current_data;
  assign current_data = input_data[current_command_index][DATA_WIDTH-1:0];

  // [Derived statements]

  logic ready_to_read_or_write = state == S_OPERATING || state == S_ERROR && beginning_of_xact;

  logic transfer;
  assign transfer =
    ready_to_read_or_write
    && (CMD_WRITE == current_command || CMD_READ == current_command);

  logic reading_or_writing;
  assign reading_or_writing = transfer || state == S_WAITING;

  logic skipping;
  assign skipping = CMD_SKIP_CYCLE == current_command;

  int unsigned cycles_left_to_skip;
  assign cycles_left_to_skip = skipping ? current_data : 0;

  // [Output driving]

  assign psel = reading_or_writing ? effective_psel : 0;
  assign penable = ~transfer;

  assign pwrite = current_command == CMD_WRITE;
  assign pwdata = pwrite ? current_data : 0;

  assign paddr = reading_or_writing ? current_addr : 0;

  // [State changes]

  state_t next_state;
  always_comb begin
    if (~presetn) begin
      next_state = S_RESETTING;
    end else begin
      case (state)
        S_INVALID: next_state = S_INVALID;
        S_RESETTING: next_state = S_OPERATING;
        S_ERROR, S_OPERATING: begin
          if (transfer) next_state = S_WAITING;
          else next_state = state;
        end
        S_WAITING: begin
          if (pready) next_state = pslverr ? S_ERROR : S_OPERATING;
          else next_state = cycles_left_until_timeout <= 1 ? S_TIMEOUT : S_WAITING;
        end
        S_TIMEOUT: next_state = presetn ? S_TIMEOUT : S_OPERATING;
        default: next_state = state;
      endcase
    end
  end

  logic command_done;
  assign command_done = skipping ?
    cycles_left_to_skip <= 1
    : next_state == S_ERROR || next_state == S_OPERATING || next_state == S_TIMEOUT;

  status_t current_status;
  int unsigned current_output;

  always_comb begin
    if (reading_or_writing && next_state == S_OPERATING) begin
      current_output = prdata;
      current_status = pslverr ? STATUS_ERROR : STATUS_OK;
    end else if (next_state == S_ERROR) begin
      current_output = 0;
      current_status = STATUS_ERROR;
    end else if (next_state == S_TIMEOUT) begin
      current_output = TIMEOUT;
      current_status = STATUS_TIMEOUT;
    end else begin
      current_output = 0;
      current_status = STATUS_OK;
    end
  end

  status_t statuses[COMMANDS_PER_BATCH];
  int unsigned output_data[COMMANDS_PER_BATCH];

  always_comb begin
    for (int i = 0; i < COMMANDS_PER_BATCH; i++) begin
      if (i == current_command_index) begin
        statuses[i] = current_status;
        output_data[i] = current_output;
      end else begin
        statuses[i] = captured_statuses[i];
        output_data[i] = captured_output_data[i];
      end
    end
  end

  function automatic void ExchangeData();
    int unsigned num_new_commands;
    int unsigned num_pushed_results;

    num_pushed_results = pull_apb.PushResults(statuses, output_data, num_results);
    if (num_pushed_results != num_results) begin
      $display("The number of pushed results does not equal to the number of requested commands");
      $finish(0);
    end

    execution_gate_i.StallIfNeeded(system_is_idle_i ? byte'(1) : byte'(0));

    num_new_commands = pull_apb.PullCommands(commands, input_addrs, input_data, COMMANDS_PER_BATCH);

    num_commands <= num_new_commands;
    num_results <= num_new_commands;
    current_command_index <= 0;
  endfunction

  initial begin
    state = S_OPERATING;
    num_commands = 0;
    current_command_index = 0;
    num_results = 0;
    psel_index = 0;
    cycles_left_until_timeout = TIMEOUT;

    for (int i = 0; i < COMMANDS_PER_BATCH; i++) begin
      commands[i] = {1'b0, CMD_NOP};
      input_addrs[i] = 0;
      input_data[i] = 0;
      captured_statuses[i] = STATUS_OK;
      captured_output_data[i] = 0;
    end

    wait (context_i != null);
    wait (execution_gate_i != null);

    pull_apb = make_edr_PullAPB(context_i, NAME, execution_gate_i);
    apb_o = pull_apb;
  end

  final begin
    if (pull_apb != null) begin
      pull_apb.delete();
    end
  end

  always @(posedge pclk) begin
    if (pull_apb != null) begin
      state <= next_state;

      if (command_done) begin
        if (CMD_SET_PSEL == current_command) psel_index <= current_data;

        if (num_commands <= 1) ExchangeData();
        else begin
          captured_statuses[current_command_index] <= current_status;
          captured_output_data[current_command_index] <= current_output;

          current_command_index <= current_command_index + 1;
          num_commands <= num_commands - 1;
        end

        cycles_left_until_timeout <= TIMEOUT;
      end else if (skipping) begin
        input_data[current_command_index] <= input_data[current_command_index] - 1;
      end else if (state == S_WAITING) begin
        cycles_left_until_timeout <= cycles_left_until_timeout - 1;
      end
    end
  end
endmodule
