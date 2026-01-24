#include<stdio.h>
//数组：
int main(){  
    //一维数组：
    int arr[10];
    //动态数组：
    int *xarr = (int*)malloc(10 * sizeof(int));   
    //二维数组：  
    int er[3][4] = {
    {1, 2, 3, 4},
    {5, 6, 7, 8},
    {9, 10, 11, 12}
};
}  
//链表：  
typedef struct Node {
    int data;
    struct Node* next;
} Node;

// 创建链表节点
Node* cn(int data) {
    Node* newN = (Node*)malloc(sizeof(Node));
    newN->data = data;
    newN->next = NULL;
    return newN;
}