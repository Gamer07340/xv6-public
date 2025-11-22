#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "elf.h"

#define MAX_LABELS 100
#define MAX_LINE 128
#define TEXT_START 0x0

typedef struct {
  char name[32];
  uint addr;
} Label;

Label labels[MAX_LABELS];
int nlabels = 0;
int pass = 0;
uint current_pc = 0;
int fd_out;
uint base_addr = 0; // base address for labels
// Helper functions
int is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

int is_digit(char c) {
  return c >= '0' && c <= '9';
}

int is_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.';
}

void skip_whitespace(char **p) {
  while (**p && is_space(**p)) (*p)++;
}

void get_token(char **p, char *token) {
  skip_whitespace(p);
  int i = 0;
  while (**p && !is_space(**p) && **p != ',') {
    token[i++] = *(*p)++;
  }
  token[i] = 0;
  if (**p == ',') (*p)++; // Skip comma
}

int parse_reg(char *s) {
  if (strcmp(s, "eax") == 0) return 0;
  if (strcmp(s, "ecx") == 0) return 1;
  if (strcmp(s, "edx") == 0) return 2;
  if (strcmp(s, "ebx") == 0) return 3;
  if (strcmp(s, "esp") == 0) return 4;
  if (strcmp(s, "ebp") == 0) return 5;
  if (strcmp(s, "esi") == 0) return 6;
  if (strcmp(s, "edi") == 0) return 7;
  return -1;
}

int parse_imm(char *s) {
  if (s[0] == '0' && s[1] == 'x') {
    // Hex
    int val = 0;
    char *p = s + 2;
    while (*p) {
      val *= 16;
      if (*p >= '0' && *p <= '9') val += *p - '0';
      else if (*p >= 'a' && *p <= 'f') val += *p - 'a' + 10;
      else if (*p >= 'A' && *p <= 'F') val += *p - 'A' + 10;
      p++;
    }
    return val;
  }
  return atoi(s);
}

void add_label(char *name, uint addr) {
  if (pass == 1) {
    if (nlabels >= MAX_LABELS) {
      printf(1, "Error: too many labels\n");
      exit();
    }
    strcpy(labels[nlabels].name, name);
    labels[nlabels].addr = addr;
    nlabels++;
  }
}

uint find_label(char *name) {
  for (int i = 0; i < nlabels; i++) {
    if (strcmp(labels[i].name, name) == 0) {
      // Labels are stored as file offsets, but need to return memory addresses
      // Since the segment is loaded at vaddr=0x1000 from file offset headers_size,
      // we subtract headers_size and add 0x1000
      uint headers_size = sizeof(struct elfhdr) + sizeof(struct proghdr);
      return labels[i].addr - headers_size + 0x1000;
    }
  }
  return 0; // Or error
}


void emit_byte(uchar b) {
  if (pass == 2) {
    write(fd_out, &b, 1);
  }
  current_pc++;
}

void emit_word(ushort w) {
  emit_byte(w & 0xFF);
  emit_byte((w >> 8) & 0xFF);
}

void emit_dword(uint d) {
  emit_byte(d & 0xFF);
  emit_byte((d >> 8) & 0xFF);
  emit_byte((d >> 16) & 0xFF);
  emit_byte((d >> 24) & 0xFF);
}

// Instruction encoding helpers
void emit_modrm(int mod, int reg, int rm) {
  emit_byte((mod << 6) | (reg << 3) | rm);
}

void process_line(char *line) {
  char *p = line;
  char token[32];
  
  skip_whitespace(&p);
  if (*p == 0 || *p == ';') return; // Empty or comment

  // Check for label definition
  char *colon = strchr(line, ':');
  if (colon) {
    *colon = 0;
    char *l = line;
    skip_whitespace(&l);
    // Trim trailing spaces
    int len = strlen(l);
    while(len > 0 && is_space(l[len-1])) l[--len] = 0;
    
    add_label(l, current_pc);
    p = colon + 1;
  }

  get_token(&p, token);
  if (token[0] == 0) return;

  // Directives
  if (strcmp(token, ".global") == 0) {
    // Ignore for now, assume entry is first instruction or main
    return;
  }
  if (strcmp(token, ".byte") == 0) {
    get_token(&p, token);
    emit_byte(parse_imm(token));
    return;
  }
  if (strcmp(token, ".long") == 0) {
    get_token(&p, token);
    emit_dword(parse_imm(token));
    return;
  }
  if (strcmp(token, ".string") == 0) {
    // Simple string parsing
    skip_whitespace(&p);
    if (*p == '"') {
      p++;
      while (*p && *p != '"') {
        if (*p == '\\') {
          p++;
          if (*p == 'n') emit_byte('\n');
          else emit_byte(*p);
        } else {
          emit_byte(*p);
        }
        p++;
      }
      emit_byte(0);
    }
    return;
  }

  // Instructions
  if (strcmp(token, "mov") == 0) {
    char op1[32], op2[32];
    get_token(&p, op1);
    get_token(&p, op2);
    int r1 = parse_reg(op1);
    int r2 = parse_reg(op2);
    
    if (r1 != -1 && r2 != -1) {
      // mov r1, r2 (r1 = r2) -> 89 /r (MOV r/m32, r32)
      // Wait, Intel syntax: mov dest, src.
      // Opcode 89: MOV r/m32, r32. MR encoding.
      // ModRM: mod=11, reg=src(r2), rm=dest(r1)
      emit_byte(0x89);
      emit_modrm(3, r2, r1);
    } else if (r1 != -1) {
      // mov r1, imm
      // B8+rd id
      emit_byte(0xB8 + r1);
      if (is_digit(op2[0]) || op2[0] == '-') {
        emit_dword(parse_imm(op2));
      } else {
        // Label
        emit_dword(find_label(op2));
      }
    }
  } else if (strcmp(token, "int") == 0) {
    char op1[32];
    get_token(&p, op1);
    emit_byte(0xCD);
    emit_byte(parse_imm(op1));
  } else if (strcmp(token, "ret") == 0) {
    emit_byte(0xC3);
  } else if (strcmp(token, "push") == 0) {
    char op1[32];
    get_token(&p, op1);
    int r1 = parse_reg(op1);
    if (r1 != -1) {
      emit_byte(0x50 + r1);
    } else {
      // push imm or label
      emit_byte(0x68);
      if (is_digit(op1[0]) || op1[0] == '-') {
        emit_dword(parse_imm(op1));
      } else {
        // Label
        emit_dword(find_label(op1));
      }
    }
  } else if (strcmp(token, "pop") == 0) {
    char op1[32];
    get_token(&p, op1);
    int r1 = parse_reg(op1);
    if (r1 != -1) {
      emit_byte(0x58 + r1);
    }
  } else if (strcmp(token, "add") == 0) {
      char op1[32], op2[32];
      get_token(&p, op1);
      get_token(&p, op2);
      int r1 = parse_reg(op1);
      int r2 = parse_reg(op2);
      if(r1 != -1 && r2 != -1) {
          // add r1, r2
          emit_byte(0x01);
          emit_modrm(3, r2, r1);
      } else if (r1 != -1) {
          // add r1, imm
          if(r1 == 0) { // add eax, imm
              emit_byte(0x05);
              emit_dword(parse_imm(op2));
          } else {
              emit_byte(0x81);
              emit_modrm(3, 0, r1);
              emit_dword(parse_imm(op2));
          }
      }
  } else if (strcmp(token, "sub") == 0) {
      char op1[32], op2[32];
      get_token(&p, op1);
      get_token(&p, op2);
      int r1 = parse_reg(op1);
      int r2 = parse_reg(op2);
      if(r1 != -1 && r2 != -1) {
          emit_byte(0x29);
          emit_modrm(3, r2, r1);
      } else if (r1 != -1) {
          if(r1 == 0) {
              emit_byte(0x2D);
              emit_dword(parse_imm(op2));
          } else {
              emit_byte(0x81);
              emit_modrm(3, 5, r1);
              emit_dword(parse_imm(op2));
          }
      }
  } else if (strcmp(token, "xor") == 0) {
      char op1[32], op2[32];
      get_token(&p, op1);
      get_token(&p, op2);
      int r1 = parse_reg(op1);
      int r2 = parse_reg(op2);
      if(r1 != -1 && r2 != -1) {
          emit_byte(0x31);
          emit_modrm(3, r2, r1);
      }
  } else if (strcmp(token, "call") == 0) {
      char op1[32];
      get_token(&p, op1);
      emit_byte(0xE8);
      uint target = find_label(op1);
      // Relative call
      uint headers_size = sizeof(struct elfhdr) + sizeof(struct proghdr);
      uint mem_pc = current_pc - headers_size + 0x1000;
      emit_dword(target - (mem_pc + 4));
  } else if (strcmp(token, "jmp") == 0) {
      char op1[32];
      get_token(&p, op1);
      emit_byte(0xE9);
      uint target = find_label(op1);
      uint headers_size = sizeof(struct elfhdr) + sizeof(struct proghdr);
      uint mem_pc = current_pc - headers_size + 0x1000;
      emit_dword(target - (mem_pc + 4));
  } else if (strcmp(token, "cmp") == 0) {
      char op1[32], op2[32];
      get_token(&p, op1);
      get_token(&p, op2);
      int r1 = parse_reg(op1);
      if (r1 == 0) { // cmp eax, imm
          emit_byte(0x3D);
          emit_dword(parse_imm(op2));
      } else if (r1 != -1) { // cmp reg, imm
          emit_byte(0x81);
          emit_modrm(3, 7, r1);
          emit_dword(parse_imm(op2));
      }
  } else if (strcmp(token, "jne") == 0) {
      char op1[32];
      get_token(&p, op1);
      emit_byte(0x0F);
      emit_byte(0x85);
      uint target = find_label(op1);
      uint headers_size = sizeof(struct elfhdr) + sizeof(struct proghdr);
      uint mem_pc = current_pc - headers_size + 0x1000;
      emit_dword(target - (mem_pc + 4));
  }
}

void assemble(char *filename) {
  char buf[MAX_LINE];
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    printf(1, "Cannot open %s\n", filename);
    exit();
  }

  // Pass 1
  pass = 1;
  uint headers_size = sizeof(struct elfhdr) + sizeof(struct proghdr);
  base_addr = headers_size; // base address for labels
  current_pc = headers_size; // Start of text section after headers
  
  // Simple line reader
  int n;
  int i = 0;
  char c;
  while ((n = read(fd, &c, 1)) > 0) {
    if (c == '\n') {
      buf[i] = 0;
      process_line(buf);
      i = 0;
    } else {
      if (i < MAX_LINE - 1) buf[i++] = c;
    }
  }
  if (i > 0) {
    buf[i] = 0;
    process_line(buf);
  }
  close(fd);

  // Save the end PC from pass 1
  uint end_pc_pass1 = current_pc;

  base_addr = headers_size; // base address for labels
  // Pass 2
  pass = 2;
  current_pc = headers_size;
  fd = open(filename, O_RDONLY);
  
  // Prepare ELF header
  struct elfhdr elf;
  struct proghdr ph;
  
  elf.magic = ELF_MAGIC;
  elf.elf[0] = 2; // 32-bit
  elf.elf[1] = 1; // Little endian
  elf.elf[2] = 1; // Version
  elf.elf[3] = 0; // ABI
  elf.type = 2; // Executable
  elf.machine = 3; // x86
  elf.version = 1;
  elf.entry = 0x1000; // Entry point at 0x1000
  elf.phoff = sizeof(struct elfhdr);
  elf.shoff = 0;
  elf.flags = 0;
  elf.ehsize = sizeof(struct elfhdr);
  elf.phentsize = sizeof(struct proghdr);
  elf.phnum = 1;
  elf.shentsize = 0;
  elf.shnum = 0;
  elf.shstrndx = 0;

  uint code_size = end_pc_pass1 - headers_size;

  
  ph.type = ELF_PROG_LOAD;
  ph.off = headers_size;
  ph.vaddr = 0x1000;
  ph.paddr = 0;
  ph.filesz = code_size;
  ph.memsz = code_size;
  ph.flags = ELF_PROG_FLAG_EXEC | ELF_PROG_FLAG_READ | ELF_PROG_FLAG_WRITE;
  ph.align = 4096;

  write(fd_out, &elf, sizeof(elf));
  write(fd_out, &ph, sizeof(ph));

  // Reset PC for pass 2
  current_pc = headers_size;



  i = 0;
  while ((n = read(fd, &c, 1)) > 0) {
    if (c == '\n') {
      buf[i] = 0;
      process_line(buf);
      i = 0;
    } else {
      if (i < MAX_LINE - 1) buf[i++] = c;
    }
  }
  if (i > 0) {
    buf[i] = 0;
    process_line(buf);
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf(1, "Usage: asm <input.asm> <output>\n");
    exit();
  }

  fd_out = open(argv[2], O_CREATE | O_RDWR);
  if (fd_out < 0) {
    printf(1, "Cannot create %s\n", argv[2]);
    exit();
  }

  assemble(argv[1]);
  close(fd_out);
  exit();
}
