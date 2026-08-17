/*
 * checksum.c
 * Simulates checksum-based error detection (sender + receiver in one program).
 * Data is split into 8-bit words, added using 1's complement addition with
 * end-around carry, and the final 1's complement of the sum is the checksum.
 *
 * Run twice from main(): once with a clean transmission, once with a
 * manually flipped bit in the transmitted data, to show both outcomes.
 */

#include <stdio.h>
#include <string.h>

#define MAX_WORDS 64

/* Adds all 8-bit words in "words[0..count-1]" using 1's complement addition
 * with end-around carry. Prints the running sum and carry at every step.
 * Returns the final 8-bit sum (before complementing). */
unsigned char add_with_end_around_carry(unsigned char words[], int count) {
    unsigned int sum = 0; /* wider than 8 bits so we can see the carry-out */

    for (int i = 0; i < count; i++) {
        sum = sum + words[i];
        int carry = 0;

        /* end-around carry: if the addition overflowed past 8 bits,
         * wrap the overflow bit back around and add it to the sum */
        if (sum > 0xFF) {
            sum = (sum & 0xFF) + 1;
            carry = 1;
        }

        printf("  step %2d: word = %3u (0x%02X)  ->  sum = %3u (0x%02X)  carry-out = %d\n",
               i + 1, words[i], words[i], sum & 0xFF, sum & 0xFF, carry);
    }

    return (unsigned char)(sum & 0xFF);
}

/* Splits a C string into 8-bit words (one word per byte/char). */
int split_into_words(const char *data, unsigned char words[]) {
    int len = strlen(data);
    for (int i = 0; i < len && i < MAX_WORDS; i++) {
        words[i] = (unsigned char)data[i];
    }
    return len;
}

/* Runs one full sender+receiver simulation for the given message.
 * If flip_bit is 1, a single bit is flipped in the transmitted data
 * (not the checksum) right before the receiver checks it. */
void run_checksum_demo(const char *message, int flip_bit) {
    unsigned char words[MAX_WORDS];
    int count = split_into_words(message, words);

    printf("Message           : \"%s\"\n", message);
    printf("Split into %d 8-bit words.\n\n", count);

    /* ---- Sender side ---- */
    printf("Sender: computing checksum\n");
    unsigned char sum = add_with_end_around_carry(words, count);
    unsigned char checksum = ~sum & 0xFF; /* 1's complement of final sum */
    printf("  final sum        = %3u (0x%02X)\n", sum, sum);
    printf("  checksum (~sum)  = %3u (0x%02X)\n\n", checksum, checksum);

    /* ---- Build the "transmitted" frame: data words + checksum word ---- */
    unsigned char frame[MAX_WORDS + 1];
    memcpy(frame, words, count);
    frame[count] = checksum;
    int frame_len = count + 1;

    if (flip_bit) {
        /* manually corrupt the data: flip bit 0 of the first data word */
        printf("Corruption: flipping bit 0 of word[0] (%3u -> ", frame[0]);
        frame[0] = frame[0] ^ 0x01;
        printf("%3u) before the receiver checks it.\n\n", frame[0]);
    }

    /* ---- Receiver side: re-sum data + checksum, same end-around carry ---- */
    printf("Receiver: re-adding data + checksum\n");
    unsigned char rsum = add_with_end_around_carry(frame, frame_len);
    printf("  final sum at receiver = %3u (0x%02X)\n\n", rsum, rsum);

    if (rsum == 0xFF) {
        printf("Verification result: ERROR-FREE (sum of all words is all 1s)\n");
    } else {
        printf("Verification result: ERROR DETECTED (sum of all words is not all 1s)\n");
    }
    printf("----------------------------------------------------------------\n\n");
}

int main(int argc, char *argv[]) {
    const char *message = (argc > 1) ? argv[1] : "NETWORKS";

    printf("=========== RUN 1: CLEAN TRANSMISSION (no corruption) ===========\n");
    run_checksum_demo(message, 0);

    printf("=========== RUN 2: CORRUPTED TRANSMISSION (bit flipped) =========\n");
    run_checksum_demo(message, 1);

    return 0;
}
