#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


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



// 1创建新节点
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

//  初始化链表
void initLinkedList(LinkedList* list) {
    list->head = NULL;
    list->size = 0;
}

//  在链表头部插入节点
void insertAtHead(LinkedList* list, int data) {
    Node* newNode = createNode(data);
    if (newNode == NULL) return;
    
    newNode->next = list->head;
    list->head = newNode;
    list->size++;
    printf("在头部插入: %d\n", data);
}

//  在链表尾部插入节点
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

//  在指定位置插入节点
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

bool deleteNode(LinkedList* list, int data) {
    if (list->head == NULL) {
        printf("链表为空，无法删除\n");
        return false;
    }
    
    Node* current = list->head;
    Node* previous = NULL;
    
    // 遍历查找要删除的节点
    while (current != NULL && current->data != data) {
        previous = current;
        current = current->next;
    }
    
    // 没有找到要删除的节点
    if (current == NULL) {
        printf("未找到值为 %d 的节点\n", data);
        return false;
    }
    
    // 删除节点
    if (previous == NULL) {
        // 删除头节点
        list->head = current->next;
    } else {
        // 删除中间或尾部节点
        previous->next = current->next;
    }
    
    printf("删除节点: %d\n", data);
    free(current);
    list->size--;
    return true;
}

//  删除指定位置的节点
bool deleteAtPosition(LinkedList* list, int position) {
    if (list->head == NULL) {
        printf("链表为空，无法删除\n");
        return false;
    }
    
    // 位置有效性检查
    if (position < 0 || position >= list->size) {
        printf("错误: 删除位置 %d 无效 (有效范围: 0-%d)\n", position, list->size - 1);
        return false;
    }
    
    Node* current = list->head;
    Node* previous = NULL;
    
    // 如果要删除头节点
    if (position == 0) {
        list->head = current->next;
        printf("删除位置 %d 的节点: %d\n", position, current->data);
        free(current);
        list->size--;
        return true;
    }
    
    // 找到要删除的节点
    for (int i = 0; i < position; i++) {
        previous = current;
        current = current->next;
    }
    
    // 删除节点
    previous->next = current->next;
    printf("删除位置 %d 的节点: %d\n", position, current->data);
    free(current);
    list->size--;
    return true;
}

// 查找节点
Node* findNode(LinkedList* list, int data) {
    Node* current = list->head;
    int position = 0;
    
    while (current != NULL) {
        if (current->data == data) {
            printf("找到节点 %d 在位置 %d\n", data, position);
            return current;
        }
        current = current->next;
        position++;
    }
    
    printf("未找到节点 %d\n", data);
    return NULL;
}


// 获取链表大小
int getSize(LinkedList* list) {
    return list->size;
}

//  判断链表是否为空
bool isEmpty(LinkedList* list) {
    return list->head == NULL;
}

// 打印链表
void printList(LinkedList* list) {
    if (list->head == NULL) {
        printf("链表为空\n");
        return;
    }
    
    Node* current = list->head;
    printf("链表 (大小: %d): ", list->size);
    
    while (current != NULL) {
        printf("%d", current->data);
        if (current->next != NULL) {
            printf(" -> ");
        }
        current = current->next;
    }
    printf(" -> NULL\n");
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