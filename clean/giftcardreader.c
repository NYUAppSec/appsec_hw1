#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "giftcard.h"

// Interpret a THX-1138 program. We have to be careful since
// this is effectively arbitrary code! We will check:
//   - All register numbers are less than NUM_REGS
//   - All accesses to the message buffer are in bounds
//   - The program counter never goes outside the code buffer
//   - The program never executes more than MAX_STEPS instructions
//     (to prevent infinite loops)
#define NUM_REGS 16
#define MAX_STEPS 100000
#define REG_OK(reg) (reg < NUM_REGS)
#define MPTR_OK() (msg <= mptr && mptr < msg + GC_PROGMSG_SIZE)
// Each instruction is 3 bytes, so the last valid instruction is at GC_PROGRAM_SIZE-3
#define PC_OK() (program <= pc && pc < program + GC_PROGRAM_SIZE - 3)
void animate(char *msg, unsigned char *program) {
    unsigned char regs[NUM_REGS];
    char *mptr = msg;
    unsigned char *pc = program;
    uint64_t steps = 0;
    bool zf = false;
    while (steps < MAX_STEPS) {
        // Ensure program counter is in bounds before fetching instruction
        if (!PC_OK()) break;
        // Decode the current instruction
        unsigned char arg1, arg2;
        enum gift_card_program_op op;
        op = *pc;
        arg1 = *(pc+1);
        arg2 = *(pc+2);
        // Execute
        switch (op) {
            case GC_OP_NOP:
                break;
            case GC_OP_GET:
                if (REG_OK(arg1) && MPTR_OK()) {
                    regs[arg1] = *mptr;
                }
                break;
            case GC_OP_PUT:
                if (REG_OK(arg1) && MPTR_OK()) {
                    *mptr = regs[arg1];
                }
                break;
            case GC_OP_MOV:
                if (MPTR_OK()) {
                    mptr += (char)arg1;
                }
                break;
            case GC_OP_CON:
                if (REG_OK(arg2)) {
                    regs[arg2] = arg1;
                }
                break;
            case GC_OP_XOR:
                if (REG_OK(arg1) && REG_OK(arg2)) {
                    regs[arg1] ^= regs[arg2];
                    zf = !regs[arg1];
                }
                break;
            case GC_OP_ADD:
                if (REG_OK(arg1) && REG_OK(arg2)) {
                    regs[arg1] += regs[arg2];
                    zf = !regs[arg1];
                }
                break;
            case GC_OP_PRN:
                printf("%.*s\n", GC_PROGMSG_SIZE, msg);
                break;
            case GC_OP_END:
                goto done;
            case GC_OP_JMP:
                pc += (char)arg1;
                break;
            case GC_OP_JCC:
                if (zf) pc += (char)arg1;
                break;
            default:
                fprintf(stderr, "invalid opcode %#02x encountered in gift card program\n", op);
                goto done;
        }
        pc += 3;
    }
done:
    return;
}

int get_gift_card_value(struct gift_card *gc) {
    int total = 0;
	for(int i=0; i < gc->number_of_gift_card_records; i++) {
        if (gc->records[i]->rec_type == GC_AMOUNT) {
            total += gc->records[i]->amount.amount;
        }
    }
    return total;
}

void print_gift_card_text(struct gift_card *gc) {
    printf("   Merchant ID: %32.*s\n", GC_MERCHANT_SIZE, gc->merchant_id);
    printf("   Customer ID: %32.*s\n", GC_CUSTOMER_SIZE, gc->customer_id);
    printf("   Num records: %d\n", gc->number_of_gift_card_records);
    for (int i = 0; i < gc->number_of_gift_card_records; i++) {
        struct gift_card_record *gcr = gc->records[i];
        printf("      record:type: %s\n", gift_card_type_str[gcr->rec_type]);
        if (gcr->rec_type == GC_AMOUNT) {
			printf("      amount_added: %d\n", gcr->amount.amount);
            if (gcr->amount.amount > 0) {
                printf("      signature: %32.*s\n", GC_SIGNATURE_SIZE, gcr->amount.signature);
            }
        }
        else if (gcr->rec_type == GC_MESSAGE) {
			printf("      message: %s\n", gcr->message.message_str);
        }
        else if (gcr->rec_type == GC_PROGRAM) {
            printf("      message: %.*s\n", GC_PROGMSG_SIZE, gcr->program.message);
            printf("  [running embedded program]\n");
            animate(gcr->program.message, gcr->program.program);
        }
        else {
            // We should never reach this because errors are checked during
            // parsing. So assert here if we do.
            assert(false && "Unknown record type in printing stage!");
        }
    }
	printf("  Total value: %d\n\n", get_gift_card_value(gc));
}

// Hex encode binary data. Note that out must have sufficient space
// to hold 2*size+1 characters (including the NULL terminator)
void hex_encode(unsigned char *data, int size, char *out) {
    const char *hexchars = "01234567890abcdef";
    for(int i = 0; i < size; i++) {
        out[i*2] = hexchars[(data[i] & 0xf0) >> 4];
        out[i*2+1] = hexchars[data[i] & 0x0f];
    }
    out[size*2] = '\0';
}

// Escapes a JSON string. Note that this dynamically allocates
// the output string, and the caller is responsible for freeing.
char *json_escape(char *s, int s_len) {
    // Conservatively assume that the output may be escaped. Since each
    // escaped character looks like \uXXXX, allocate 6 times the size of
    // the input string, plus 1 for NULL terminator.
    char *output = calloc(6 * s_len + 1, 1);
    char *output_ptr = output;
    if (!output) {
        // If we fail here we can't really recover, so just exit
        perror("calloc");
        exit(1);
    }
    for (int i = 0; i < s_len; i++) {
        int bytes_written = 0;
        switch (s[i]) {
            case '"':
                bytes_written = sprintf(output_ptr, "\\\"");
                break;
            case '\\':
                bytes_written = sprintf(output_ptr, "\\\\");
                break;
            case '\b': 
                bytes_written = sprintf(output_ptr, "\\b");
                break;
            case '\f':
                bytes_written = sprintf(output_ptr, "\\f");
                break;
            case '\n':
                bytes_written = sprintf(output_ptr, "\\n");
                break;
            case '\r':
                bytes_written = sprintf(output_ptr, "\\r");
                break;
            case '\t':
                bytes_written = sprintf(output_ptr, "\\t");
                break;
            default:
                if ('\x00' <= s[i] && s[i] <= '\x1f') {
                    bytes_written = sprintf(output_ptr, "\\u%04d", (int)s[i]);
                }
                else if ((unsigned char)s[i] > '\x7f') {
                    // These aren't valid string characters, so drop them
                    bytes_written = 0;
                }
                else {
                    // Character can go directly in the output
                    *output_ptr = s[i];
                    bytes_written = 1;
                }
        }
        output_ptr += bytes_written;
    }
    // NULL-terminate
    *output_ptr = '\0';
    return output;
}

// Output in JSON format. Somewhat messy because we have to ensure
// any strings we print are properly escaped.
void print_gift_card_json(struct gift_card *gc) {
    char *escaped = NULL;
    printf("{\n");
    escaped = json_escape(gc->merchant_id, GC_MERCHANT_SIZE);
    printf("  \"merchant_id\": \"%32s\",\n", escaped);
    free(escaped);
    escaped = json_escape(gc->customer_id, GC_CUSTOMER_SIZE);
    printf("  \"customer_id\": \"%32s\",\n", escaped);
    free(escaped);
    printf("  \"total_value\": %d,\n", get_gift_card_value(gc));
    printf("  \"records\": [\n");
    for (int i = 0; i < gc->number_of_gift_card_records; i++) {
        struct gift_card_record *gcr = gc->records[i];
        printf("    {\n");
        printf("      \"record_type\": \"%s\",\n", gift_card_type_str[gcr->rec_type]);
        if (gcr->rec_type == GC_AMOUNT) {
            printf("      \"amount_added\": %d,\n",gcr->amount.amount);
            if (gcr->amount.amount > 0) {
                escaped = json_escape(gcr->amount.signature, GC_SIGNATURE_SIZE);
                printf("      \"signature\": \"%32s\"\n", escaped);
                free(escaped);
            }
        }
        else if (gcr->rec_type == GC_MESSAGE) {
            escaped = json_escape(gcr->message.message_str, strlen(gcr->message.message_str));
			printf("      \"message\": \"%s\"\n", escaped);
            free(escaped);
        }
        else if (gcr->rec_type == GC_PROGRAM) {
            escaped = json_escape(gcr->program.message, GC_PROGMSG_SIZE);
			printf("      \"message\": \"%s\",\n", escaped);
            free(escaped);
            char program_hex[GC_PROGRAM_SIZE*2+1];
            hex_encode(gcr->program.program, GC_PROGRAM_SIZE, program_hex);
            printf("      \"program\": \"%s\"\n", program_hex);
        }
        else {
            // We should never reach this because errors are checked during
            // parsing. So assert here if we do.
            assert(false && "Unknown record type in printing stage!");
        }

        // JSON forbids trailing commas
        if (i < gc->number_of_gift_card_records-1)
            printf("    },\n");
        else
            printf("    }\n");
    }
    printf("  ]\n");
    printf("}\n");
}

void free_gift_card(struct gift_card *gc) {
    // Free everything associated with a gift card
    if (!gc) return;
    if (gc->records) {
        for (int i = 0; i < gc->number_of_gift_card_records; i++) {
            struct gift_card_record *gcr = gc->records[i];
            if (gcr) {
                // The only one that has dynamically allocated data is the
                // GC_MESSAGE type, so handle it specially
                if (gcr->rec_type == GC_MESSAGE) {
                    if(gcr->message.message_str) free(gcr->message.message_str);
                    gcr->message.message_str = NULL;
                }
                free(gcr);
                gcr = NULL;
                gc->records[i] = NULL;
            }
        }
        free(gc->records);
        gc->records = NULL;
    }
    free(gc);
}

/* Parses the file into an in-memory data structure 
 * Parameters:
 *    fp: an open FILE pointer
 * Return value:
 *    a struct gift_card pointer representing the parsed data,
 *    or NULL if an error occurred during parsing
 */
struct gift_card * parse_gift_card(FILE *fp) {
    struct gift_card *gc = NULL;

    // We don't want to rely on the sizes in the file, so instead
    // we will get the filesize from the OS here.
    if (-1 == fseek(fp, 0, SEEK_END)) {
        perror("fseek");
        return NULL;
    }
    long file_size = ftell(fp);
    if (file_size < 0) {
        perror("ftell");
        return NULL;
    }
    rewind(fp);

    // Keep track of how much we've parsed
    long bytes_remaining = file_size;

    // Size as reported by the file
    uint32_t reported_file_size = 0;
    if (1 != fread(&reported_file_size, sizeof(uint32_t), 1, fp)) {
        perror("fread");
        goto error_ret;
    }
    // Check that it actually matches, to detect corrupt files
    if (reported_file_size != file_size) {
        fprintf(stderr, "error: file size on disk (%ld) does not match size in header (%d), aborting\n",
                file_size, reported_file_size);
        goto error_ret;
    }
    bytes_remaining -= sizeof(uint32_t);

    gc = calloc(sizeof(struct gift_card), 1);
    if (!gc) {
        goto error_ret;
    }

    // Header info: merchant and customer IDs
    if (1 != fread(gc->merchant_id, GC_MERCHANT_SIZE, 1, fp)) {
        perror("fread");
        goto error_ret;
    }
    bytes_remaining -= GC_MERCHANT_SIZE;
    if (1 != fread(gc->customer_id, GC_CUSTOMER_SIZE, 1, fp)) {
        perror("fread");
        goto error_ret;
    }
    bytes_remaining -= GC_CUSTOMER_SIZE;

    // Get number of records reported. We won't use this but we will check
    // at the end to make sure it matches.
    uint32_t num_records = 0;
    if (1 != fread(&num_records, sizeof(uint32_t), 1, fp)) {
        perror("fread");
        goto error_ret;
    }
    bytes_remaining -= sizeof(uint32_t);

    // Main record parse loop. Keep going until we run out of data
    // or hit EOF
    while (!feof(fp) && bytes_remaining > 0) {
        uint32_t rec_size = 0;
        uint32_t rec_type = 0;
        if (1 != fread(&rec_size, sizeof(uint32_t), 1, fp)) {
            perror("fread");
            goto error_ret;
        }
        bytes_remaining -= sizeof(uint32_t);
        if (1 != fread(&rec_type, sizeof(uint32_t), 1, fp)) {
            perror("fread");
            goto error_ret;
        }
        bytes_remaining -= sizeof(uint32_t);

        // Grow the array so we can store a pointer to this record
        gc->number_of_gift_card_records++;
        gc->records = realloc(gc->records, gc->number_of_gift_card_records * sizeof(struct gift_card_record));
        if (!gc->records) {
            perror("realloc");
            goto error_ret;
        }
        // Allocate the record itself. Since we are using a union each record is the same
        // size, so we can just allocate it here.
        int idx = gc->number_of_gift_card_records - 1;
        gc->records[idx] = calloc(sizeof(struct gift_card_record), 1);
        if (!gc->records[idx]) {
            perror("calloc");
            // We didn't actually successfully allocate, so reduce the number of records
            // by one so that the cleanup function doesn't try to free it
            gc->number_of_gift_card_records--;
            goto error_ret;
        }
        
        // Read in the record
        struct gift_card_record *gcr = gc->records[idx];
        gcr->rec_type = rec_type;
        if (gcr->rec_type == GC_AMOUNT) {
            if (1 != fread(&gcr->amount.amount, sizeof(int32_t), 1, fp)) {
                perror("fread");
                goto error_ret;
            }
            bytes_remaining -= sizeof(int32_t);

            if (gcr->amount.amount >= 0) {
                if (1 != fread(gcr->amount.signature, GC_SIGNATURE_SIZE, 1, fp)) {
                    perror("fread");
                    goto error_ret;
                }
                bytes_remaining -= GC_SIGNATURE_SIZE;
            }
        }
        else if (gcr->rec_type == GC_MESSAGE) {
            // Slightly tricky since we don't trust the record size to be accurate. 
            // Take the min of the declared record size and the amount of data
            // left in the file to read.
            // Also note that the rec_size includes the record header, which is
            // two uint32_t fields (8 bytes)
            int message_len = 0;
            int rec_remaining = rec_size - 2*sizeof(uint32_t);
            if (rec_remaining > bytes_remaining) {
                message_len = bytes_remaining;
            }
            else {
                message_len = rec_remaining;
            }
            gcr->message.message_str = calloc(message_len, 1);
            if (!gcr->message.message_str) {
                perror("calloc");
                goto error_ret;
            }
            if (1 != fread(gcr->message.message_str, message_len, 1, fp)) {
                perror("fread");
                goto error_ret;
            }
            bytes_remaining -= message_len;
            // Ensure the string is NULL-terminated
            gcr->message.message_str[message_len-1] = '\0';
        }
        else if (gcr->rec_type == GC_PROGRAM) {
            if (1 != fread(gcr->program.message, GC_PROGMSG_SIZE, 1, fp)) {
                perror("fread");
                goto error_ret;
            }
            bytes_remaining -= GC_PROGMSG_SIZE;
            if (1 != fread(gcr->program.program, GC_PROGRAM_SIZE, 1, fp)) {
                perror("fread");
                goto error_ret;
            }
            bytes_remaining -= GC_PROGRAM_SIZE;
        }
        else {
            fprintf(stderr, "unknown record type encountered: %d; aborting\n", gcr->rec_type);
            goto error_ret;
        }
    }

    // Check that the number of records we read is correct
    if (gc->number_of_gift_card_records != num_records) {
        fprintf(stderr, "Number of records reported in file (%u) does not match"
                        "number actually read (%d). Aborting.\n",
                        num_records, gc->number_of_gift_card_records);
        goto error_ret;
    }

    // If we get here, everything is okay! Return the gift card data.
    return gc;

error_ret:
    // Free resources in case of error
    free_gift_card(gc);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <1|2> <filename>\n", argv[0]);
        fprintf(stderr, "   use 1 for text output, 2 for json output\n");
        return 1;
    }

    FILE *fp = fopen(argv[2], "rb");
    if (!fp) {
        fprintf(stderr, "couldn't open %s\n", argv[2]);
        return 1;
    }

    struct gift_card *gc = parse_gift_card(fp);
    if (!gc) {
        fprintf(stderr, "error reading gift card %s\n", argv[2]);
        return 1;
    }

    // Print either text or JSON format
    if (strcmp(argv[1], "1") == 0) {
        print_gift_card_text(gc);
    }
    else if (strcmp(argv[1], "2") == 0) {
        print_gift_card_json(gc);
    }
    else {
        fprintf(stderr, "invalid output format: %s\n", argv[1]);
        free_gift_card(gc);
        return 1;
    }

    free_gift_card(gc);
    return 0;
}
