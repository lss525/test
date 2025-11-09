#include <stdio.h>
struct Year {
    int year;
    int months[12];
};
int totalDays(struct Year *yearData, int month) {
    if (month < 1 || month > 12) {
        return -1;
    }
    int total = 0;
    for (int i = 0; i < month; i++) {
        total += yearData->months[i];
    }
    return total;
}
int main() {
    struct Year year2024 = {
        2024,
        {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
    };
    int month;
    scanf("%d", &month);
    int result = totalDays(&year2024, month);
    if (result != -1) {
        printf("%d %d %d\n", year2024.year, month, result);
    }
    return 0;
}