#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <elf.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <getopt.h>
#include "processor.h"

using namespace std;

extern void single_cycle_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc);
extern void pipelined_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc, int width);
extern void processor_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc, int width);

/* Load Binary. */
uint32_t load(char *bmk, Memory &memory)
{
  Elf32_Ehdr ehdr;
  Elf32_Shdr shdr;

  /* Open binary executable. */
  FILE *binary, *binary_copy;
  binary = fopen(bmk, "r");
  if (!binary) {
      cout << "Failed to open executable binary: " << string(optarg) << "\n";
      return 0;
  }

  /* Read and verify executable header. */
  int num_read = fread(&ehdr, 1, sizeof(ehdr), binary);
  if ((num_read != sizeof(ehdr)) || memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7)) {
     cout << "Error in ELF header\n";
     return 0;
  }

  /* Read section headers. */
  fseek(binary, ehdr.e_shoff, SEEK_SET);
  for (int i = 0; i < ehdr.e_shnum; i++) {
      num_read = fread(&shdr, sizeof(shdr), 1, binary);
      if (num_read != 1) {
          cout << "Error in section header: " << num_read << " shdr=" << shdr.sh_addr << "\n";
          break;
      }
      uint32_t word, dummy_word;
      if ((shdr.sh_flags & SHF_EXECINSTR) != 0 && shdr.sh_addr == 0) { /* Text section -- we hardcoded this to zero during compilation. */
          binary_copy = fopen(bmk, "r");
          fseek(binary_copy, shdr.sh_offset, SEEK_SET);
          for (int j = 0; j < (int)shdr.sh_size; j += 4) {
              num_read = fread(&word, 1, 4, binary_copy);
              if (num_read != 4) {
                  cout << "Could not populate memory from section: " << shdr.sh_addr <<
                          ": bytes read=" << j + num_read << ", section header size=" << shdr.sh_size << "\n";
                  return 0;
              }
              memory.access((uint32_t)shdr.sh_addr+j, dummy_word, word, false, true);
          }
          fclose(binary_copy);
          return shdr.sh_size;
      }
  }

  return 0;
}

void print_help()
{
    cout << "Required Options.\n" 
            "--bmk <path-to-executable>           Path to the benchmark executable binary.\n"
            "Optional:\n"
            "--help                               Print this help message\n"
            "-O0                                  Optimization Level 0 (single-cycle processor)\n"
            "-O1                                  Optimization Level 1 (pipelined processor)\n"
            "-O2                                  Optimization Level 2 (branch predictor)\n"
            "-O3                                  Optimization Level 3 (custom optimization TBD; includes O2)\n"
            "-O4                                  Optimization Level 4 (custom optimization TBD; includes O3)\n"
            "                                     Defaults to -O0\n";
}

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
      {"bmk", required_argument, 0, 'b'},
      {"opt", optional_argument, 0, 'O'},
      {"opt0", optional_argument, 0, '0'},
      {"opt1", optional_argument, 0, '1'},
      {"opt2", optional_argument, 0, '2'},
      {"opt3", optional_argument, 0, '3'},
      {"opt4", optional_argument, 0, '4'},
      {"help", no_argument, 0, 'h'}
    };
    int option_index = 0;
    bool initialized = false;

    Memory memory;
    Processor processor(&memory); 
    uint32_t end_pc = 0;

    int optLevel = 0;

    while (true) {
      char c = getopt_long(argc, argv, "b:O01234h", long_options, &option_index);
      if (c == -1) {
          if (!initialized) {
              print_help();
              exit(0);
          }
          break;
      }
      switch (c) {
          default :
          case 'h':
              print_help();
              exit(0);
          case 'b':
              end_pc = load(optarg, memory);
              break;
          case 'O':
              break;
          case '0':
          case '1':
          case '4':
              optLevel = c - '0';
              processor.initialize(optLevel);
              initialized = true;
              break;
          case '2':
              optLevel = 2;
              processor.enableBranchPrediction();
              processor.initialize(optLevel);
              initialized = true;
              break;
          case '3':
              optLevel = 3;
              processor.initialize(optLevel);
              initialized = true;
              break;
      }
    }

    memory.setOptLevel(optLevel);
    uint64_t num_cycles = 0;
    
    while (processor.getPC() <= end_pc) {
        processor.advance();
        cout << "\nCYCLE " << num_cycles << "\n";
        processor.printRegFile();
        num_cycles++;
    }

    int drain_cycles = 0;
    for (int i = 0; i < drain_cycles; i++) {
        processor.advance();
        cout << "\nCYCLE " << num_cycles << "\n";
        processor.printRegFile();
        num_cycles++;
    }


    cout << "\nCompleted execution in " << (double)num_cycles*(optLevel ? 1 : 125)*0.5 << " nanoseconds.\n";
}
