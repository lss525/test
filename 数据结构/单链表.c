#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// ==================== 数据结构定义 ====================

// 链表节点结构
typedef struct Node {
    int data;           // 数据域
    struct Node* next;  // 指向下一个节点的指针
} Node;

// 链表结构（包含头指针和大小）
typedef struct {
    Node* head;  // 头指针
    int size;    // 链表大小
} LinkedList;

// ==================== 基本操作函数 ====================

// 1. 创建新节点
Node* createNode(int data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        printf("内存分配失败!\n");
        return NULL;
    }
    newNode->data = data;
    newNode->next = NULL;
    return newNode;
}

// 2. 初始化链表
void initLinkedList(LinkedList* list) {
    list->head = NULL;
    list->size = 0;
}

// 3. 在链表头部插入节点
void insertAtHead(LinkedList* list, int data) {
    Node* newNode = createNode(data);
    if (newNode == NULL) return;
    
    newNode->next = list->head;
    list->head = newNode;
    list->size++;
    printf("在头部插入: %d\n", data);
}

// 4. 在链表尾部插入节点
void insertAtTail(LinkedList* list, int data) {
    Node* newNode = createNode(data);
    if (newNode == NULL) return;
    
    // 如果链表为空
    if (list->head == NULL) {
        list->head = newNode;
    } else {
        // 找到最后一个节点
        Node* current = list->head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
    }
    list->size++;
    printf("在尾部插入: %d\n", data);
}

// 5. 在指定位置插入节点
bool insertAtPosition(LinkedList* list, int data, int position) {
    // 位置有效性检查
    if (position < 0 || position > list->size) {
        printf("错误: 插入位置 %d 无效 (有效范围: 0-%d)\n", position, list->size);
        return false;
    }
    
    // 在头部插入
    if (position == 0) {
        insertAtHead(list, data);
        return true;
    }
    
    // 创建新节点
    Node* newNode = createNode(data);
    if (newNode == NULL) return false;
    
    // 找到插入位置的前一个节点
    Node* current = list->head;
    for (int i = 0; i < position - 1; i++) {
        current = current->next;
    }
    
    // 插入节点
    newNode->next = current->next;
    current->next = newNode;
    list->size++;
    
    printf("在位置 %d 插入: %d\n", position, data);
    return true;
}


int main() {
    int choice;
    
    printf("选择运行模式:\n");
    printf("1. 运行完整测试\n");
    printf("2. 运行交互式菜单\n");
    printf("选择: ");
    scanf("%d", &choice);
    
    if (choice == 1) {
        testLinkedList();
    } else if (choice == 2) {
        menuSystem();
    } else {
        printf("无效选择，运行测试模式\n");
        testLinkedList();
    }
    
    return 0;
}