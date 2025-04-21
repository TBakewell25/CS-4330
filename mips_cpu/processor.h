#include "memory.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"

#ifdef ENABLE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x) 
#endif


class Processor{
	//forwarding unit
					
    private:
	bool branch_prediction_enabled;
        static const int BP_SIZE = 128;  // Size of branch predictor table
        
	bool stall = false; 
	unsigned int cache_penalty_mem = 0;
	unsigned int cache_penalty_fetch = 0;

	int opt_level;
	ALU alu;
	control_t control;
	Memory *memory;
	Registers regfile;

	uint32_t processor_pc = 0;
	//add other structures as needed
	//pipelined processor

        struct branch_entry {
                uint32_t address;
                unsigned int taken;
        };
    
        branch_entry branch_predictor[BP_SIZE];   // predictor table
	
        struct IF_ID{
		uint32_t instruction; //obvious
		uint32_t pc;
	};
	
	struct ID_EX{
                uint32_t instruction;

		int opcode;
		int rs, rt, rd;

		uint32_t shamt; //?
		uint32_t funct; //function bits
		uint32_t imm; //immediate value
		uint32_t addr; //address value

		uint32_t read_data_1, read_data_2; //product of
	
		control_t control; //preserve control across signals cycles
		uint32_t pc;
	};
	

	struct EX_MEM{
		
		uint32_t imm; //persits imm from previous stage

		uint32_t read_data_1, read_data_2; //persist from prev stage
		int rd, rt; //registers
		uint32_t write_data;
		//uint32_t operand_1, operand_2; //operands from prev stage
		uint32_t alu_zero; //same
		//uint32_t addr; //address for mem access
		uint32_t alu_result;
			
		control_t control; //preserve control across signals cycles
		uint32_t pc;
	};
	
	struct MEM_WB{
		int write_reg; //reg to write to
		uint32_t write_data; //data to write
		
		uint32_t imm;
		//uint32_t addr;
		uint32_t alu_zero;
		//uint32_t read_data_1, read_data_2;
		
		control_t control; //preserve control across signals cycles
		uint32_t pc;	
	};
	
	//allow access to correct pipeline registers across
	//function contexts

	struct pipelineState{
		IF_ID fetchDecode;
		ID_EX decExe;
		EX_MEM exeMem;
		MEM_WB memWrite;
	};

	pipelineState state;
	pipelineState prevState; //store the previous state so we can simulate shared state across contexts
	
		//add private functions
	void single_cycle_processor_advance();
	void pipelined_processor_advance();

	branch_entry lookup_branch_prediction(uint32_t pc) {
	    int index = (pc >> 2) % BP_SIZE; // Use lower bits of PC to index into the table
	    return (branch_predictor[index]);  // Predict taken if counter is 2 or 3
	}

	void update_branch_prediction(uint32_t pc, bool is_taken, uint32_t target) {
	    int index = (pc >> 2) % BP_SIZE;
            branch_predictor[index].address = target;
	    if (is_taken) {
		if (branch_predictor[index].taken < 3) {
		    branch_predictor[index].taken++;
		}
	    } else {
		if (branch_predictor[index].taken > 0) {
		    branch_predictor[index].taken --;
		}
	    }
	}

	void detect_data_hazard(){
		// detect load/use hazard
		if (prevState.decExe.control.reg_write && prevState.decExe.control.mem_read){
			// For I-type instructions - check if source register in decode matches destination in execute
		  	if (state.decExe.rs == prevState.decExe.rt){
				stall = true;
				return;
			}
		  
		  	// For R-type instructions - check both source registers
		  	if (state.decExe.rt == prevState.decExe.rt){
				stall = true;
				return;
			}
	 	}
	 	return;
	}
	//possible forwarding unit inputs:
	//prevState.exeDec.rd
	//prevState.memWrite.write_reg
	//prevState.decExe.rs
	//prevState.decExe.rt
	//prevState.exeMem.control.reg_write
	//prevState.memWrite.control.regWrite

	//forwarding notes:
	//control signal regdest picks between prevState rd and rt, not tied to forwarding
	//alu_src needs to pick between inputs in exe

	int get_forwarding_a(){
		if (!prevState.decExe.rs)
			return 0;  //use read_data_1

		//Forward from MEM stage
		if (prevState.exeMem.control.reg_write &&  //reg_write signal set
			((prevState.exeMem.rt ==  prevState.decExe.rs) || //I type	
			//prevState.exeMem.rd != 0 && 
			(prevState.exeMem.rd == prevState.decExe.rs))){  //R type
				return 2; //forward from mem
		} 
		if (prevState.memWrite.control.reg_write &&
			(prevState.memWrite.write_reg == prevState.decExe.rs))
				return 1;

		return 0; //base case, no forwarding required
	}

	int get_forwarding_b(){
		if (!prevState.decExe.rt) //rt changed to rd
			return 0; //use read_data_2

		//Forward from MEM stage
		if (prevState.exeMem.control.reg_write &&  //reg_write signal set
				((prevState.exeMem.rt == prevState.decExe.rt) || //I type
				(prevState.exeMem.rd == prevState.decExe.rt))) //R type
					return 2; //forward from mem

		//Forward from WB stage
		if (prevState.memWrite.control.reg_write &&
			(prevState.memWrite.write_reg == prevState.decExe.rt))
				return 1;
		return 0;
	}

	void detect_control_hazard(control_t control){
	    if (control.branch || control.bne) {
		// Determine if branch is actually taken
		bool branch_taken = false;
		
		if (control.branch && !control.bne) {
		    // BEQ (Branch if Equal)
		    branch_taken = (state.exeMem.alu_zero == 1);
		} else if (control.bne) {
		    // BNE (Branch if Not Equal)
		    branch_taken = (state.exeMem.alu_zero == 0);
		}

                if (branch_taken && !branch_prediction_enabled) {
                     clear_IF_ID();
                     clear_ID_EX();
                     processor_pc = state.exeMem.pc + 4 + (state.exeMem.imm << 2);
                 }
	
                if (branch_prediction_enabled) {	
			// Calculate actual branch target
			uint32_t actual_target = branch_taken ? 
			    state.exeMem.pc + 4 + (state.exeMem.imm << 2) : 
			    state.exeMem.pc + 4;
			    
			// Get the branch instruction address
			uint32_t branch_instr_addr = state.exeMem.pc;
		
                        branch_entry predicted = lookup_branch_prediction(branch_instr_addr);
			bool was_predicted_taken = (predicted.taken >= 2);
			
			// If prediction was wrong, we need to flush and redirect
			if (was_predicted_taken != branch_taken) {
			    clear_IF_ID();
			    clear_ID_EX();
			    processor_pc = actual_target;
			}	
			// Update the branch predictor with actual outcome
			update_branch_prediction(branch_instr_addr, branch_taken, actual_target);
	//NOTE: need to fix order, currently updates before checking prediction		
			// Check if our prediction was wrong
			
                 }
                 
	    }

	    if (control.jump){
		clear_IF_ID();
		clear_ID_EX();
	
		if (control.jump_reg){
			// jr instruction - jump to register value
			processor_pc = prevState.decExe.read_data_1;
		} else{
			// j/jal instruction - construct target address
			// IMPORTANT: Use OR (|) not AND (&) to combine the high bits with address
			processor_pc = (prevState.decExe.pc & 0xf0000000) | (prevState.decExe.addr << 2);
		}
	
	    // Debug the new PC value
		DEBUG(cout << "JUMP TARGET: PC=0x" << hex << processor_pc << dec << endl);
	    }	
	}

    public:

	Processor(Memory *mem){ regfile.pc = 0; memory = mem;}

	uint32_t getPC(){ return regfile.pc;}

	//Prints the Register File
	void printRegFile(){ regfile.print(); }
	
	//Initializes the processor appropriately based on the optimization level
	void initialize(int opt_level);

	//Advances the processor to an appropriate state every cycle
	void advance(); 

	//Pipeline stages as functions, should be self explanatory

	void pipelined_fetch();

	void pipelined_decode();

	void pipelined_execute();

	void pipelined_mem();

	void pipelined_wb();

	void flush_pipeline();
        
        void enableBranchPrediction() {
            branch_prediction_enabled = true;
	    for (int i = 0; i < BP_SIZE; i++) {
		branch_predictor[i].taken = 1;
                branch_predictor[i].address = NULL;
	     }
	}
	
	void clear_ID_EX(){
		state.decExe.opcode = 0;
		state.decExe.rs = 0;
		state.decExe.rt = 0;
		state.decExe.rd = 0;
		state.decExe.shamt = 0;
		state.decExe.funct = 0;
		state.decExe.imm = 0;
		state.decExe.addr = 0;
		state.decExe.read_data_1 = 0;
		state.decExe.read_data_2 = 0;
		
		state.decExe.pc = 0;	
		state.decExe.control.reset();
	}

	void clear_IF_ID(){ 
		state.fetchDecode.pc = 0;
		state.fetchDecode.instruction = 0; }
};;
