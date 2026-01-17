module safe (
  input  logic  CLK50,
  input  logic [9:0] SW,
  input  logic [1:0] KEY,             
  output logic [9:0] LEDR,
  output logic [7:0] HEX5, HEX4, HEX3, HEX2, HEX1, HEX0
);

  //open/locked display
  localparam logic [47:0] OPEN_WORD   = 48'hFC_C0_8C_86_AB_F7;
  localparam logic [47:0] LOCKED_WORD = 48'hC7_C0_C6_89_86_C0;

  logic [9:0] PASSWORD;
  logic [9:0] ATTEMPT;

 
  logic MATCH;
  logic [9:0] diff; //Gets difference in bits of attempt and actuall password
  logic [3:0] HINT; //displays hint on last 3 

  //will get from fsm_gates or synth
  logic [1:0] PRESENT_STATE;
  logic       LOCKED;
  logic       savePW;
  logic       saveAT;

  /*fsm_gates u_fsm (
    .CLK50         (CLK50),
    .reset         (KEY[0]),            
    .E             (~KEY[1]),           
    .M             (MATCH),             
    .PRESENT_STATE (PRESENT_STATE),
    .LOCKED        (LOCKED),
    .savePW        (savePW),
    .saveAT        (saveAT)
  );
  */
  
   fsm_synth u_fsm (
    .CLK50         (CLK50),
    .reset         (KEY[0]),            
    .E             (~KEY[1]),           
    .M             (MATCH),             
    .PRESENT_STATE (PRESENT_STATE),
    .LOCKED        (LOCKED),
    .savePW        (savePW),
    .saveAT        (saveAT)
  );

  assign MATCH = (ATTEMPT == PASSWORD);
  
  always_comb begin
   
    {HEX5, HEX4, HEX3, HEX2, HEX1, HEX0} = LOCKED_WORD; //initiate with locked
    HINT  = 4'd0; //hint is just 0 rn

    // OPEN vs LOCKED word
    if (!LOCKED) begin
      {HEX5, HEX4, HEX3, HEX2, HEX1, HEX0} = OPEN_WORD;
    end

    // stores different bits using xor of SW and PASSWORD, (1 and 0 will mean diff=1)
    diff = SW ^ PASSWORD;

    if (LOCKED) begin
      int count;
      count = 0;
		//basically going through all 10 bits from diff, and seeing which ones are 1, increasing the count of unmatched switches
      for (int i = 0; i < 10; i++) begin
        if (diff[i] == 1)
          count++;
      end
      HINT = count[3:0]; //stores number of incorrect switches as a binary number shown on the leds.
    end
	 
	 // show state on LEDs (present state and hint are calculated in always_comb, thats why assigning here)
    LEDR[9:8] = PRESENT_STATE;
    LEDR[7:4] = 4'b0000;
    LEDR[3:0] = HINT;
	 
  end

  always_ff @(posedge CLK50) begin
    if (~KEY[0]) begin
      PASSWORD <= 10'b0000000000;   // after reset, set a known password to 0
      ATTEMPT  <= 10'b1111111111;   // any value is fine, used only for first compare
    end else begin
      if (savePW)
			PASSWORD <= SW;   
      if (saveAT) 
			ATTEMPT  <= SW;   
    end
  end

endmodule
