#include "predictor.h"

/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////

uint8_t predictor_2bitsat[4096];

void InitPredictor_2bitsat() {
  // initialize state to weakly not taken (01)
  for (int i = 0; i < 4096; i++) {
    predictor_2bitsat[i] = 1;
  }
}

bool GetPrediction_2bitsat(UINT32 PC) {
  // get the index of table from PC
  // index is PC[13:2]
  int index = (PC >> 2) & 0xFFF;
  uint8_t state = predictor_2bitsat[index];

  // MSB of state determines if branch T or N
  return (state >> 1) & 0x1;
}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  int index = (PC >> 2) & 0xFFF;
  
  if (resolveDir == TAKEN){
    if (predictor_2bitsat[index] < 3){
      predictor_2bitsat[index]++;
    }
  }
  else if (resolveDir == NOT_TAKEN){
    if (predictor_2bitsat[index] > 0){
      predictor_2bitsat[index]--;
    }
  }
}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////
/*
There is one BHT table with 512 entries. 
There are 8 PHT tables with 64 entries each
The 3 LSB of the PC points to a PHT table
The BHT table entry points an entry of the chosen PHT table
*/

uint8_t predictor_2level_bht[512];
uint8_t predictor_2level_pht[8][64];

void InitPredictor_2level() {
  for (int i = 0; i < 512; i++){
    predictor_2level_bht[i] = 0; // assume history is not taken
  }

  for (int j = 0; j < 8; j++){
    for (int k = 0; k < 64; k++) {
      predictor_2level_pht[j][k] = 1;
    }
  }
}

bool GetPrediction_2level(UINT32 PC) {
  int bht_index = ((PC >> 3) & 0x1FF);
  int pht_index = ((PC) & 0x7);

  int history = predictor_2level_bht[bht_index]; 
  int state = predictor_2level_pht[pht_index][history];

  return (state >> 1) & 0x1;
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  int bht_index = ((PC >> 3) & 0x1FF);
  int pht_index = ((PC) & 0x7);
  int history = predictor_2level_bht[bht_index]; 
 
  // update the pht
  if (resolveDir == TAKEN){
    if (predictor_2level_pht[pht_index][history] < 3){
      predictor_2level_pht[pht_index][history]++;
    }
  }
  else if (resolveDir == NOT_TAKEN){
    if (predictor_2level_pht[pht_index][history] > 0){
      predictor_2level_pht[pht_index][history]--;
    }
  }

  // update the history for the given branch
  predictor_2level_bht[bht_index] = ((history << 1) | (resolveDir ? 1 : 0)) & 0x3F;
}

/////////////////////////////////////////////////////////////
// openend
/////////////////////////////////////////////////////////////
/*
pSHARE
8 bits from the PC is used to index the history table
  There is one history table with 256 entries
  this is a private history table

8 bits from the PC is used to XOR with history[PC[7:0]]
  we will use this to index into a 3 bit counter with 2^8 entries

gSHARE
8 bits from the PC is used to XOR with global 8-bit history buffer
  we will use this to index into a 3 bit counter with 2^8 entries

selector
8 bits from the PC is used to access a selector with 2^8 entries
  this will inform us on which predictor's result to choose from
*/

/******* pSHARE ***********/

// uint8_t open_predictor_priv_history_tbl[256];
// uint8_t open_predictor_priv_pred_tbl[256];

// void InitPredictor_openend() {
//   for (int i = 0; i < 256; i++){
//     open_predictor_priv_history_tbl[i] = 0; // assume history is not taken
//   }

//   for (int j = 0; j < 256; j++){
//     open_predictor_priv_pred_tbl[j] = 1; // intialize to weak not taken
//   }
// }

// bool GetPrediction_openend(UINT32 PC) {
//   int history_tbl_index = ((PC >> 8) & 0xFF);
//   int history = open_predictor_priv_history_tbl[history_tbl_index];

//   int pred_tbl_index = (PC & 0xFF) ^ history;
//   int state = open_predictor_priv_pred_tbl[pred_tbl_index];

//   return (state >> 2) & 0x1;
// }

// void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {

//   // update prediction table
//   int history_tbl_index = ((PC >> 8) & 0xFF);
//   int history = open_predictor_priv_history_tbl[history_tbl_index];
//   int pred_tbl_index = (PC & 0xFF) ^ history;

//   if (resolveDir == TAKEN){
//     if (open_predictor_priv_pred_tbl[pred_tbl_index] < 7){
//       open_predictor_priv_pred_tbl[pred_tbl_index]++;
//     }
//   }
//   else if (resolveDir == NOT_TAKEN){
//     if (open_predictor_priv_pred_tbl[pred_tbl_index] > 0){
//       open_predictor_priv_pred_tbl[pred_tbl_index]--;
//     }
//   }

//   // update the history for the given branch
//   open_predictor_priv_history_tbl[history_tbl_index] = ((history << 1) | (resolveDir ? 1 : 0)) & 0xFF;
// }

/******* gSHARE ***********/

uint16_t open_predictor_global_history;  // 15 bit global history
uint8_t open_predictor_pred_tbl[32768];

void InitPredictor_openend() {
  open_predictor_global_history = 0; // assume history is not taken

  for (uint16_t j = 0; j < 32768; j++){
    open_predictor_pred_tbl[j] = 3; // intialize to weak not taken
  }
}

bool GetPrediction_openend(UINT32 PC) {
  uint16_t index = (PC & 0x7FFF) ^ open_predictor_global_history;
  uint8_t state = open_predictor_pred_tbl[index];

  return (state >> 2) & 0x1;
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {

  // update prediction table
  uint16_t index = (PC & 0x7FFF) ^ open_predictor_global_history;

  if (resolveDir == TAKEN){
    if (open_predictor_pred_tbl[index] < 7){
      open_predictor_pred_tbl[index]++;
    }
  }
  else if (resolveDir == NOT_TAKEN){
    if (open_predictor_pred_tbl[index] > 0){
      open_predictor_pred_tbl[index]--;
    }
  }

  // update global 
  open_predictor_global_history = ((open_predictor_global_history << 1) | (resolveDir ? 1 : 0)) & 0x7FFF;
}

/******* combined ***********/
// pSHARE
// uint8_t open_predictor_priv_history_tbl[256];
// uint8_t open_predictor_priv_pred_tbl[256];

// // gSHARE
// uint8_t open_predictor_global_history;  // 8 bit global history
// uint8_t open_predictor_global_pred_tbl[256];

// // selector
// uint8_t selector[256];

// void InitPredictor_openend() {
//   // pSHARE
//   for (int i = 0; i < 256; i++){
//     open_predictor_priv_history_tbl[i] = 0; // assume history is not taken
//     open_predictor_priv_pred_tbl[i] = 3; // intialize to weak not taken
//   }

//   // gSHARE
//   open_predictor_global_history = 0; // assume history is not taken

//   for (int j = 0; j < 256; j++){
//     open_predictor_global_pred_tbl[j] = 3; // intialize to weak not taken
//     selector[j] = 1; // inialize to using pShare
//   }
// }

// bool GetPrediction_openend(UINT32 PC) {
//   int state;

//   // pSHARE
//   int history_tbl_index = ((PC >> 8) & 0xFF); // PC[15:7]
//   int history = open_predictor_priv_history_tbl[history_tbl_index];
//   int pshare_pred_tbl_index = (PC & 0xFF) ^ history;

//   // gSHARE
//   int gshare_pred_tbl_index = (PC & 0xFF) ^ open_predictor_global_history;

//   // select prediction
//   uint8_t index = (PC >> 16) & 0xFF;
//   uint8_t select = selector[index];

//   if ((select >> 1) & 0x1) {
//     state = open_predictor_global_pred_tbl[gshare_pred_tbl_index];
//   }
//   else {
//     state = open_predictor_priv_pred_tbl[pshare_pred_tbl_index];
//   }

//   return (state >> 2) & 0x1;
// }

// void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
//   // pSHARE
//   int history_tbl_index = ((PC >> 8) & 0xFF);
//   int history = open_predictor_priv_history_tbl[history_tbl_index];
//   int pshare_pred_tbl_index = (PC & 0xFF) ^ history;

//   if (resolveDir == TAKEN){
//     if (open_predictor_priv_pred_tbl[pshare_pred_tbl_index] < 7){
//       open_predictor_priv_pred_tbl[pshare_pred_tbl_index]++;
//     }
//   }
//   else if (resolveDir == NOT_TAKEN){
//     if (open_predictor_priv_pred_tbl[pshare_pred_tbl_index] > 0){
//       open_predictor_priv_pred_tbl[pshare_pred_tbl_index]--;
//     }
//   }
//   // update the history for the given branch
//   open_predictor_priv_history_tbl[history_tbl_index] = ((history << 1) | (resolveDir ? 1 : 0)) & 0xFF;

//   // gSHARE
//   // update prediction table
//   int gshare_pred_tbl_index = (PC & 0xFF) ^ open_predictor_global_history;

//   if (resolveDir == TAKEN){
//     if (open_predictor_global_pred_tbl[gshare_pred_tbl_index] < 7){
//       open_predictor_global_pred_tbl[gshare_pred_tbl_index]++;
//     }
//   }
//   else if (resolveDir == NOT_TAKEN){
//     if (open_predictor_global_pred_tbl[gshare_pred_tbl_index] > 0){
//       open_predictor_global_pred_tbl[gshare_pred_tbl_index]--;
//     }
//   }
//   // update global 
//   open_predictor_global_history = ((open_predictor_global_history << 1) | (resolveDir ? 1 : 0)) & 0xFF;

//   // update selector
//   bool pshare_pred = open_predictor_priv_pred_tbl[pshare_pred_tbl_index] >= 4;
//   bool gshare_pred = open_predictor_global_pred_tbl[gshare_pred_tbl_index] >= 4;
//   uint8_t index = (PC >> 16) & 0xFF;
//   uint8_t sel = selector[index];

//   // if pShare correct and gShare wrong → bias toward pShare
//   if (pshare_pred == resolveDir && gshare_pred != resolveDir) {
//       if (sel > 0) selector[index]--;
//   }
//   // if gShare correct and pShare wrong → bias toward gShare
//   else if (gshare_pred == resolveDir && pshare_pred != resolveDir) {
//       if (sel < 3) selector[index]++;
//   }
// }


/******* combined pSHARE with 2 level ***********/
// pSHARE
// 12 bits from the PC is used to index the history table
//   There is one history table with 4096 entries
//   this is a private history table

// 8 bits from the PC is used to XOR with history[PC[7:0]]
//   we will use this to index into a 3 bit counter with 2^8 entries

// pSHARE
// uint8_t open_predictor_priv_history_tbl[4096];
// uint8_t open_predictor_priv_pred_tbl[4096];

// // 2level
// uint8_t open_predictor_2level_bht[4096];
// uint8_t open_predictor_2level_pht[8][64];

// // selector
// uint8_t selector[4096];

// void InitPredictor_openend() {
//   // pSHARE
//   for (int i = 0; i < 4096; i++){
//     open_predictor_priv_history_tbl[i] = 0; // assume history is not taken
//     open_predictor_priv_pred_tbl[i] = 1; // intialize to weak not taken
//   }

//   // 2level
//   for (int i = 0; i < 4096; i++){
//     open_predictor_2level_bht[i] = 0; // assume history is not taken
//   }

//   for (int j = 0; j < 8; j++){
//     for (int k = 0; k < 64; k++) {
//       open_predictor_2level_pht[j][k] = 1;
//     }
//   }

//   // selector
//   for (int m = 0; m < 4096; m++){
//     selector[m] = 1; // initialize to pSHARE
//   }

// }

// bool GetPrediction_openend(UINT32 PC) {
//   int state;

//   // pSHARE
//   int history_tbl_index = ((PC >> 12) & 0xFFF);
//   int history = open_predictor_priv_history_tbl[history_tbl_index];
//   int pshare_pred_tbl_index = (PC & 0xFFF) ^ history;

//   // 2level
//   int bht_index = ((PC >> 3) & 0xFFF); 
//   int pht_index = PC & 0x7;
//   int twolevel_history = open_predictor_2level_bht[bht_index]; 

//   // select prediction
//   uint8_t index = (PC) & 0xFFF; // 12 LSB of PC
//   uint8_t select = selector[index];

//   if ((select >> 1) & 0x1) {
//     state = open_predictor_2level_pht[pht_index][twolevel_history];
//   }
//   else {
//     state = open_predictor_priv_pred_tbl[pshare_pred_tbl_index];
//   }

//   return (state >> 1) & 0x1;
// }

// void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
//   // pSHARE
//   int history_tbl_index = ((PC >> 12) & 0xFFF);
//   int history = open_predictor_priv_history_tbl[history_tbl_index];
//   int pshare_pred_tbl_index = (PC & 0xFFF) ^ history;

//   // 2level
//   int bht_index = ((PC >> 3) & 0xFFF);
//   int pht_index = PC & 0x7;
//   int twolevel_history = open_predictor_2level_bht[bht_index]; 

//   // update selector
//   bool pshare_pred = open_predictor_priv_pred_tbl[pshare_pred_tbl_index] >= 4;
//   bool twolevel_pred = open_predictor_2level_pht[pht_index][twolevel_history] >= 4;
//   uint8_t index = PC & 0xFFF;
//   uint8_t sel = selector[index];

//   // if pShare correct and gShare wrong → bias toward pShare
//   if (pshare_pred == resolveDir && twolevel_pred != resolveDir) {
//       if (sel > 0) selector[index]--;
//   }
//   // if 2level correct and pShare wrong → bias toward 2level
//   else if (twolevel_pred == resolveDir && pshare_pred != resolveDir) {
//       if (sel < 3) selector[index]++;
//   }

//   if (resolveDir == TAKEN){
//     if (open_predictor_priv_pred_tbl[pshare_pred_tbl_index] < 3){
//       open_predictor_priv_pred_tbl[pshare_pred_tbl_index]++;
//     }
//   }
//   else if (resolveDir == NOT_TAKEN){
//     if (open_predictor_priv_pred_tbl[pshare_pred_tbl_index] > 0){
//       open_predictor_priv_pred_tbl[pshare_pred_tbl_index]--;
//     }
//   }
//   // update the history for the given branch
//   open_predictor_priv_history_tbl[history_tbl_index] = ((history << 1) | (resolveDir ? 1 : 0)) & 0xFFF;

//   // 2level
//   // update the pht
//   if (resolveDir == TAKEN){
//     if (open_predictor_2level_pht[pht_index][twolevel_history] < 3){
//       open_predictor_2level_pht[pht_index][twolevel_history]++;
//     }
//   }
//   else if (resolveDir == NOT_TAKEN){
//     if (open_predictor_2level_pht[pht_index][twolevel_history] > 0){
//       open_predictor_2level_pht[pht_index][twolevel_history]--;
//     }
//   }

//   // update the history for the given branch
//   open_predictor_2level_bht[bht_index] = ((twolevel_history << 1) | (resolveDir ? 1 : 0)) & 0x3F;
// }