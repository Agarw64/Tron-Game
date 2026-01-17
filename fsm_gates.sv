module fsm_gates (
	input  logic CLK50,       
	input  logic reset,    
	input  logic E, // ENTER
	input  logic M, // MATCH (1 when ATTEMPT == PASSWORD)
	output logic [1:0] PRESENT_STATE, 
	output logic LOCKED, 
	output logic savePW,    
	output logic saveAT    
);

  // present-state bits that match fsm design
  logic Q1, Q0;
  assign Q1 = PRESENT_STATE[1];
  assign Q0 = PRESENT_STATE[0];

  // next-state bits from equations
  logic Q1_next, Q0_next;
  assign Q0_next = E;
  assign Q1_next = (Q1 & ~Q0)|
						 (~Q1 & Q0 & ~E)|    
                   (Q1 & Q0 & E)|     
                   (Q1 & Q0 & ~M);      
						 
  // Moore outputs (depend only on our present states)
  always_comb begin
    LOCKED = Q1;          
    savePW = (~Q1 & Q0); 
    saveAT = ( Q1 & Q0); 
  end

  //synchronous reset to OPENSTATE = 2'b00
  always_ff @(posedge CLK50) begin
    if (~reset)
      PRESENT_STATE <= 2'b00;  // OPENSTATE
    else
      PRESENT_STATE[1] <= Q1_next;
	   PRESENT_STATE[0] <= Q0_next;
  end

endmodule
