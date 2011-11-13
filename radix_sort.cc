#include <stdint.h>
#include <string.h>

void lsd_radix_sort(uint32_t *left, uint32_t *right)
{
    int width = right - left;
    uint32_t *scratch = new uint32_t[width];
    uint32_t *cur = left, *other = scratch;
    uint32_t counts[256];
    /*
     * We do four passes
     * (0) input -> scratch
     * (1) scratch -> input
     * (2) input -> scratch
     * (3) scratch -> input
     *
     * So after the fourth pass, the input array is sorted and back in
     * the original storage.
     */

    for (int digit = 0; digit < 4; digit++) {
        memset(counts, 0, sizeof counts);
        for (int i = 0; i < width; i++) {
            counts[(cur[i] >> (8 * digit)) & 0xFF]++;
        }
        int total = 0;
        for (int i = 0; i < 256; i++) {
            int tmp = counts[i];
            counts[i] = total;
            total += tmp;
        }
        for (int i = 0; i < width; i++) {
            int d = (cur[i] >> (8 * digit)) & 0xFF;
            other[counts[d]++] = cur[i];
        }
        uint32_t *tmp = cur;
        cur = other;
        other = tmp;
    }

    delete[] scratch;
}
