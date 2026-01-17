module fsm_synth (
  input  logic       CLK50,
  input  logic       reset,      
  input  logic       E,
  input  logic       M,
  output logic [1:0] PRESENT_STATE,
  output logic       LOCKED,
  output logic       savePW,
  output logic       saveAT
);

  typedef enum logic [1:0] {
    OPENSTATE   = 2'b00,
    OPENHOLD    = 2'b01,
    LOCKEDSTATE = 2'b10,
    LOCKEDHOLD  = 2'b11
  } state_t;

  state_t ps, ns;                
  assign PRESENT_STATE = ps;      

  // outputs (Moore)
  always_comb begin
    LOCKED = 1'b0;
    savePW = 1'b0;
    saveAT = 1'b0;

  case (ps)
    OPENHOLD:    savePW = 1'b1;
    LOCKEDSTATE: LOCKED = 1'b1;
    LOCKEDHOLD:  begin LOCKED = 1'b1; saveAT = 1'b1; end
    endcase
  end

  // next state
  always_comb begin
  ns = ps;

  case (ps)
    OPENSTATE:    if (E)  ns = OPENHOLD;
    OPENHOLD:     if (!E) ns = LOCKEDSTATE;
    LOCKEDSTATE:  if (E)  ns = LOCKEDHOLD;
    LOCKEDHOLD:   if (!E) ns = (M ? OPENSTATE : LOCKEDSTATE);
    endcase
  end

  
  always_ff @(posedge CLK50) begin
    if (~reset) ps <= OPENSTATE;
    else        ps <= ns;
  end

endmodule


