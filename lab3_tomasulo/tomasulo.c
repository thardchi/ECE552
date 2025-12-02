
#include <limits.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"
#include "decode.def"

#include "instr.h"

/* PARAMETERS OF THE TOMASULO'S ALGORITHM */
#define INSTR_QUEUE_SIZE 16 // (10)

#define RESERV_INT_SIZE 5 // (4)
#define RESERV_FP_SIZE 3  // (2)
#define FU_INT_SIZE 3     // (2)
#define FU_FP_SIZE 1

#define FU_INT_LATENCY 5 // (4)
#define FU_FP_LATENCY 7  // (9)

/* IDENTIFYING INSTRUCTIONS */

// unconditional branch, jump or call
#define IS_UNCOND_CTRL(op) (MD_OP_FLAGS(op) & F_CALL || \
                            MD_OP_FLAGS(op) & F_UNCOND)

// conditional branch instruction
#define IS_COND_CTRL(op) (MD_OP_FLAGS(op) & F_COND)

// floating-point computation
#define IS_FCOMP(op) (MD_OP_FLAGS(op) & F_FCOMP)

// integer computation
#define IS_ICOMP(op) (MD_OP_FLAGS(op) & F_ICOMP)

// load instruction
#define IS_LOAD(op) (MD_OP_FLAGS(op) & F_LOAD)

// store instruction
#define IS_STORE(op) (MD_OP_FLAGS(op) & F_STORE)

// trap instruction
#define IS_TRAP(op) (MD_OP_FLAGS(op) & F_TRAP)

#define USES_INT_FU(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_STORE(op))
#define USES_FP_FU(op) (IS_FCOMP(op))

#define WRITES_CDB(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_FCOMP(op))

/* FOR DEBUGGING */

// prints info about an instruction
#define PRINT_INST(out, instr, str, cycle)    \
  myfprintf(out, "%d: %s", cycle, str);       \
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n", instr->index);

#define PRINT_REG(out, reg, str, instr)       \
  myfprintf(out, "reg#%d %s ", reg, str);     \
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n", instr->index);

/* VARIABLES */

// instruction queue for tomasulo
static instruction_t *instr_queue[INSTR_QUEUE_SIZE];
// number of instructions in the instruction queue
static int instr_queue_size = 0;

// reservation stations (each reservation station entry contains a pointer to an instruction)
static instruction_t *reservINT[RESERV_INT_SIZE];
static instruction_t *reservFP[RESERV_FP_SIZE];

// functional units
static instruction_t *fuINT[FU_INT_SIZE];
static instruction_t *fuFP[FU_FP_SIZE];

// common data bus
static instruction_t *commonDataBus = NULL;

// The map table keeps track of which instruction produces the value for each register
static instruction_t *map_table[MD_TOTAL_REGS];

// the index of the last instruction fetched
static int fetch_index = 0;

/* FUNCTIONAL UNITS */

/* RESERVATION STATIONS */

/* ECE552 Assignment 3 - BEGIN CODE */
// helper functions
int get_free_rs_entry(instruction_t **rs, int size);
void update_map_table(instruction_t *instr, instruction_t **map_table);
void remove_instr_from_ifq(instruction_t **instr_queue, int *instr_queue_size);
void issue_rdy_instr(instruction_t **rs, int rs_size, int current_cycle);
bool instr_dispatched(instruction_t *instr, int current_cycle);
bool instr_issued(instruction_t *instr, int current_cycle);
bool instr_ready_to_execute(instruction_t *instr, int current_cycle);
bool instr_executed(instruction_t *instr, int current_cycle, int latency);
void sort_instr(instruction_t **instr_array, int instr_count);
void allocate_fu(instruction_t **ready_instr, int ready_count, instruction_t **fu, int fu_size, int current_cycle);
void free_entry(instruction_t **rs, int rs_size, instruction_t *instr);

enum fu_type
{
  INT,
  FP
};
/* ECE552 Assignment 3 - END CODE */

/*
 * Description:
 * 	Checks if simulation is done by finishing the very last instruction
 *      Remember that simulation is done only if the entire pipeline is empty
 * Inputs:
 * 	sim_insn: the total number of instructions simulated
 * Returns:
 * 	True: if simulation is finished
 */
static bool is_simulation_done(counter_t sim_insn)
{

  /* ECE552: YOUR CODE GOES HERE */
  /*
    simulation is done if all instructions have been feteched
    and if the IFQ, CBD, RS, and FU are empty
  */
  if (sim_insn > fetch_index)
  {
    return false;
  }
  if (instr_queue_size > 0)
  {
    return false;
  }
  if (commonDataBus != NULL)
  {
    return false;
  }
  for (int i = 0; i < RESERV_INT_SIZE; i++)
  {
    if (reservINT[i] != NULL)
    {
      return false;
    }
  }
  for (int i = 0; i < RESERV_FP_SIZE; i++)
  {
    if (reservFP[i] != NULL)
    {
      return false;
    }
  }
  for (int i = 0; i < FU_INT_SIZE; i++)
  {
    if (fuINT[i] != NULL)
    {
      return false;
    }
  }
  for (int i = 0; i < FU_FP_SIZE; i++)
  {
    if (fuFP[i] != NULL)
    {
      return false;
    }
  }
  return true;
}

/*
 * Description:
 * 	Retires the instruction from writing to the Common Data Bus
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void CDB_To_retire(int current_cycle)
{

  /* ECE552: YOUR CODE GOES HERE */
  if (commonDataBus == NULL)
  {
    return;
  }
  // clear the CDB
  commonDataBus = NULL;
}

/*
 * Description:
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle)
{

  /* ECE552: YOUR CODE GOES HERE */
  // find instructions that finish executing this cycle
  instruction_t *completing_int[FU_INT_SIZE];
  instruction_t *completing_fp[FU_FP_SIZE];
  int completing_int_count = 0;
  int completing_fp_count = 0;

  // check int fu
  for (int i = 0; i < FU_INT_SIZE; i++)
  {
    if (fuINT[i] != NULL)
    {
      instruction_t *instr = fuINT[i];
      if (instr_executed(instr, current_cycle, FU_INT_LATENCY))
      {
        // if store instr,
        if (IS_STORE(instr->op))
        {
          // don't braodcast on cbd
          instr->tom_cdb_cycle = 0;
          // free rs entry
          free_entry(reservINT, RESERV_INT_SIZE, instr);
          // free fu entry
          fuINT[i] = NULL;
        }
        else
        {
          // add to list of instr that need to be broadcast
          completing_int[completing_int_count++] = instr;
        }
      }
    }
  }

  // check fp fu
  for (int i = 0; i < FU_FP_SIZE; i++)
  {
    if (fuFP[i] != NULL)
    {
      instruction_t *instr = fuFP[i];
      if (instr_executed(instr, current_cycle, FU_FP_LATENCY))
      {
        completing_fp[completing_fp_count++] = instr;
      }
    }
  }

  // find oldest instr and its index in the FU
  instruction_t *oldest_instr = NULL;
  int fu_index = -1;
  enum fu_type fu_type = 0;

  // find the oldest completed instr
  for (int i = 0; i < completing_int_count; i++)
  {
    if (oldest_instr == NULL || completing_int[i]->index < oldest_instr->index)
    {
      oldest_instr = completing_int[i];
      fu_type = INT;
      for (int j = 0; j < FU_INT_SIZE; j++)
      {
        if (fuINT[j] == oldest_instr)
        {
          fu_index = j;
          break;
        }
      }
    }
  }

  for (int i = 0; i < completing_fp_count; i++)
  {
    if (oldest_instr == NULL || completing_fp[i]->index < oldest_instr->index)
    {
      oldest_instr = completing_fp[i];
      fu_type = FP;
      for (int j = 0; j < FU_FP_SIZE; j++)
      {
        if (fuFP[j] == oldest_instr)
        {
          fu_index = j;
          break;
        }
      }
    }
  }

  // boardcast if instr availble and CBD free
  if (commonDataBus == NULL && oldest_instr != NULL)
  {
    commonDataBus = oldest_instr;
    oldest_instr->tom_cdb_cycle = current_cycle;
    // if oldest instr is an int, free int rs and fu
    if (fu_type == INT)
    {
      free_entry(reservINT, RESERV_INT_SIZE, oldest_instr);
      fuINT[fu_index] = NULL;
    }
    // if oldest instr is an fp, free fp rs and fu
    else if (fu_type == FP)
    {
      free_entry(reservFP, RESERV_FP_SIZE, oldest_instr);
      fuFP[fu_index] = NULL;
    }
  }
}

/*
 * Description:
 * 	Moves instruction(s) from the issue to the execute stage (if possible). We prioritize old instructions
 *      (in program order) over new ones, if they both contend for the same functional unit.
 *      All RAW dependences need to have been resolved with stalls before an instruction enters execute.
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void issue_To_execute(int current_cycle)
{

  /* ECE552: YOUR CODE GOES HERE */
  instruction_t *ready_int[RESERV_INT_SIZE];
  instruction_t *ready_fp[RESERV_FP_SIZE];
  int ready_int_count = 0;
  int ready_fp_count = 0;

  // find all integer instructions ready to execute
  for (int i = 0; i < RESERV_INT_SIZE; i++)
  {
    if (reservINT[i] != NULL)
    {
      instruction_t *instr = reservINT[i];
      // check if instruction is in issue stage and RAW dependencies are resolved
      if (instr_ready_to_execute(instr, current_cycle))
      {
        ready_int[ready_int_count++] = instr;
      }
    }
  }

  // find all FP instructions ready to execute
  for (int i = 0; i < RESERV_FP_SIZE; i++)
  {
    if (reservFP[i] != NULL)
    {
      instruction_t *instr = reservFP[i];
      // check if instruction is in issue stage and RAW dependencies are resolved
      if (instr_ready_to_execute(instr, current_cycle))
      {
        ready_fp[ready_fp_count++] = instr;
      }
    }
  }

  // sort ready integer instructions
  sort_instr(ready_int, ready_int_count);
  // sort ready FU intstructions
  sort_instr(ready_fp, ready_fp_count);

  // allocate int fu entry
  allocate_fu(ready_int, ready_int_count, fuINT, FU_INT_SIZE, current_cycle);
  // allocate fp fu entry
  allocate_fu(ready_fp, ready_fp_count, fuFP, FU_FP_SIZE, current_cycle);
}

/*
 * Description:
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void dispatch_To_issue(int current_cycle)
{

  /* ECE552: YOUR CODE GOES HERE */
  // check all int reservation stations
  for (int entry = 0; entry < RESERV_INT_SIZE; entry++)
  {
    if (reservINT[entry] != NULL)
    {
      instruction_t *instr = reservINT[entry];

      if (instr_dispatched(instr, current_cycle))
      {
        instr->tom_issue_cycle = current_cycle;
      }
    }
  }

  // check all fp reservation stations
  for (int entry = 0; entry < RESERV_FP_SIZE; entry++)
  {
    if (reservFP[entry] != NULL)
    {
      instruction_t *instr = reservFP[entry];

      if (instr_dispatched(instr, current_cycle))
      {
        instr->tom_issue_cycle = current_cycle;
      }
    }
  }
}

/*
 * Description:
 * 	Grabs an instruction from the instruction trace (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	None
 */
void fetch(instruction_trace_t *trace)
{

  /* ECE552: YOUR CODE GOES HERE */

  // the IFQ is full
  if (instr_queue_size >= INSTR_QUEUE_SIZE)
    return;
  // we've fetched all availble instructions
  if (fetch_index == sim_num_insn)
    return;

  // move to  the next instruction
  fetch_index++;
  while (fetch_index < sim_num_insn)
  {
    instruction_t *instr = get_instr(trace, fetch_index);
    // add instruction to queque if it not a trap
    if (!IS_TRAP(instr->op))
    {
      instr_queue[instr_queue_size] = instr;
      instr_queue_size++;
      return;
    }
    // else skip to next instrction
    fetch_index++;
  }

  // Only traps remained
  fetch_index = sim_num_insn;
}

/*
 * Description:
 * 	Calls fetch and dispatches an instruction at the same cycle (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void fetch_To_dispatch(instruction_trace_t *trace, int current_cycle)
{
  /* ECE552: YOUR CODE GOES HERE */

  int old_size = instr_queue_size;
  fetch(trace);

  // if we fetched a new instruction, set its dispatch cycle
  if (instr_queue_size > old_size)
  {
    instr_queue[instr_queue_size - 1]->tom_dispatch_cycle = current_cycle;
  }

  // nothing to dispatch if IFQ empty
  if (instr_queue_size == 0)
  {
    return;
  }

  // get instruction at the head of the IFQ
  instruction_t *instr = instr_queue[0];

  // conditional instructions don't use RS or FU, so can dispatch and remove from IFQ
  if (IS_UNCOND_CTRL(instr->op) || IS_COND_CTRL(instr->op))
  {
    // remove from IFQ
    remove_instr_from_ifq(instr_queue, &instr_queue_size);
    return;
  }

  // instructuction uses FU
  if (USES_INT_FU(instr->op))
  {
    int rs_idx = get_free_rs_entry(reservINT, RESERV_INT_SIZE);
    if (rs_idx != -1)
    {
      // allocate rs entry
      reservINT[rs_idx] = instr;

      // update map table
      update_map_table(instr, map_table);

      // remove insturction from IFQ
      remove_instr_from_ifq(instr_queue, &instr_queue_size);
    }
  }
  else if (USES_FP_FU(instr->op))
  {
    int rs_idx = get_free_rs_entry(reservFP, RESERV_FP_SIZE);
    if (rs_idx != -1)
    {
      // allocate rs entry
      reservFP[rs_idx] = instr;

      // update map table
      update_map_table(instr, map_table);

      // remove instruction from IFQ
      remove_instr_from_ifq(instr_queue, &instr_queue_size);
    }
  }

  // if no reservation station available, stall
}

/*
 * Description:
 * 	Performs a cycle-by-cycle simulation of the 4-stage pipeline
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	The total number of cycles it takes to execute the instructions.
 * Extra Notes:
 * 	sim_num_insn: the number of instructions in the trace
 */
counter_t runTomasulo(instruction_trace_t *trace)
{
  instr_queue_size = 0;
  fetch_index = 0;
  commonDataBus = NULL;
  // initialize instruction queue
  int i;
  for (i = 0; i < INSTR_QUEUE_SIZE; i++)
  {
    instr_queue[i] = NULL;
  }

  // initialize reservation stations
  for (i = 0; i < RESERV_INT_SIZE; i++)
  {
    reservINT[i] = NULL;
  }

  for (i = 0; i < RESERV_FP_SIZE; i++)
  {
    reservFP[i] = NULL;
  }

  // initialize functional units
  for (i = 0; i < FU_INT_SIZE; i++)
  {
    fuINT[i] = NULL;
  }

  for (i = 0; i < FU_FP_SIZE; i++)
  {
    fuFP[i] = NULL;
  }

  // initialize map_table to no producers
  int reg;
  for (reg = 0; reg < MD_TOTAL_REGS; reg++)
  {
    map_table[reg] = NULL;
  }

  int cycle = 1;
  while (true)
  {

    /* ECE552: YOUR CODE GOES HERE */
    CDB_To_retire(cycle);

    // Stage 4: Move from Execute to CDB
    execute_To_CDB(cycle);

    // Stage 3: Move from Issue to Execute
    issue_To_execute(cycle);

    // Stage 2: Move from Dispatch to Issue
    dispatch_To_issue(cycle);

    // Stage 1: Fetch and Dispatch
    fetch_To_dispatch(trace, cycle);

    cycle++;

    if (is_simulation_done(sim_num_insn))
      break;
  }

  return cycle;
}

/* ECE552 Assignment 3 - BEGIN CODE */

// helper function that searches for a free rs entry and returns index
int get_free_rs_entry(instruction_t **rs, int size)
{
  for (int idx = 0; idx < size; idx++)
  {
    if (rs[idx] == NULL)
    {
      return idx;
    }
  }
  return -1; // no free rs entries
}

// helper function to update RAW dependencies and map table
void update_map_table(instruction_t *instr, instruction_t **map_table)
{
  // Mark RAW dependencies using map table
  for (int i = 0; i < 3; i++)
  {
    if (instr->r_in[i] != DNA && instr->r_in[i] != 0)
    {
      instr->Q[i] = map_table[instr->r_in[i]];
    }
    else
    {
      instr->Q[i] = NULL;
    }
  }

  // Update map table for output registers
  for (int i = 0; i < 2; i++)
  {
    if (instr->r_out[i] != DNA && instr->r_out[i] != 0)
    {
      map_table[instr->r_out[i]] = instr;
    }
  }
}

// helper function to remove insturuction from ifq
void remove_instr_from_ifq(instruction_t **instr_queue, int *instr_queue_size)
{
  if (*instr_queue_size <= 0)
  {
    return; // nothing to remove
  }
  else
  {
    for (int i = 0; i < *instr_queue_size - 1; i++)
    {
      instr_queue[i] = instr_queue[i + 1];
    }
    *instr_queue_size = *instr_queue_size - 1;
    instr_queue[*instr_queue_size] = NULL;
    return;
  }
}

// helper function to dispatch valid instructions
void issue_rdy_instr(instruction_t **rs, int rs_size, int current_cycle)
{
  for (int entry = 0; entry < rs_size; entry++)
  {
    if (rs[entry] != NULL)
    {
      instruction_t *instr = rs[entry];

      if (instr->tom_dispatch_cycle < current_cycle && instr->tom_dispatch_cycle > 0 && instr->tom_issue_cycle == 0)
      {
        instr->tom_issue_cycle = current_cycle;
      }
    }
  }
  return;
}

// helper function to check if instr has dispatched
bool instr_dispatched(instruction_t *instr, int current_cycle)
{
  return (instr->tom_dispatch_cycle < current_cycle && instr->tom_dispatch_cycle > 0 && instr->tom_issue_cycle == 0);
}

// helper function to check if instr has issued
bool instr_issued(instruction_t *instr, int current_cycle)
{
  return (instr->tom_issue_cycle < current_cycle && instr->tom_issue_cycle > 0 && instr->tom_execute_cycle == 0);
}

// helper function to check if instr executed
bool instr_executed(instruction_t *instr, int current_cycle, int latency)
{
  return (current_cycle >= instr->tom_execute_cycle + latency && instr->tom_execute_cycle > 0);
}

// helper function to check if all instr opearands are available
bool instr_ready_to_execute(instruction_t *instr, int current_cycle)
{
  if (instr == NULL)
    return false;

  if (!instr_issued(instr, current_cycle))
    return false;

  for (int j = 0; j < 3; j++)
  {
    if (instr->Q[j] != NULL)
    {
      // Dependency not resolved if producer hasn't completed CDB
      if (instr->Q[j]->tom_cdb_cycle == 0 || instr->Q[j]->tom_cdb_cycle >= current_cycle)
      {
        return false;
      }
    }
  }
  return true;
}

// helper function to sort by oldest instructions
void sort_instr(instruction_t **instr_array, int instr_count)
{
  if (instr_count == 0)
    return;

  for (int i = 0; i < instr_count - 1; i++)
  {
    for (int j = i + 1; j < instr_count; j++)
    {
      if (instr_array[i]->index > instr_array[j]->index)
      {
        instruction_t *temp = instr_array[i];
        instr_array[i] = instr_array[j];
        instr_array[j] = temp;
      }
    }
  }
}

// helper function to allocate FU
void allocate_fu(instruction_t **ready_instr, int ready_count, instruction_t **fu, int fu_size, int current_cycle)
{
  int num_fu_entry_allocated = 0;
  for (int i = 0; i < ready_count && num_fu_entry_allocated < fu_size; i++)
  {
    instruction_t *instr = ready_instr[i];

    for (int j = 0; j < fu_size; j++)
    {
      if (fu[j] == NULL)
      {
        fu[j] = instr;
        instr->tom_execute_cycle = current_cycle;

        // Clear Q dependencies (no longer needed in execute)
        for (int k = 0; k < 3; k++)
        {
          instr->Q[k] = NULL;
        }

        // IMPORTANT: Do NOT free the reservation station here!
        // The instruction keeps its RS entry until it gets the CDB
        // This was the key change from before

        num_fu_entry_allocated++;
        break;
      }
    }
  }
  return;
}

// helper function to free RS entry
void free_entry(instruction_t **table, int size, instruction_t *instr)
{
  for (int i = 0; i < size; i++)
  {
    if (table[i] == instr)
    {
      table[i] = NULL;
      break;
    }
  }
  return;
}

/* ECE552 Assignment 3 - END CODE */