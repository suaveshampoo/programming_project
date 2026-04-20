#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum{
    BUY = 0,
    SELL = 1
} type;

typedef struct Modify{
    int id;
    double price;
    int quantity;
    int time;
} Modify;

typedef struct Order{
    int id;
    char symbol[8];
    type type;
    double price;
    int quantity;
    int time;
    int ttl;
    struct Order *next;
} Order; 

typedef struct{
    Order *buy_head;
    Order *sell_head;
} OrderBook;

int validate_ADD(const Order *o, OrderBook *book, int *current_time);
int parse_ADD(const char* line, Order *o);
int parse_CANCEL(const char* line, int *id, int *time);
int validate_CANCEL(int id, int time, int n,OrderBook *book, int *current_time);
int cancel_order(OrderBook *book, int id);
int remove_from_list(Order **head, int id);
int parse_MODIFY(const char* line, Modify *modify, OrderBook *book, int *current_time);
void match_buy(OrderBook *book, Order *buy);
void match_sell(OrderBook *book, Order *sell);
Order* modify_order(OrderBook *book, Modify *modify);
void expired_orders(OrderBook *book, int current_time);
int parse_QUERY(const char *line, int *time);
void command_query(OrderBook* book);
void free_list(Order *head);

int main(int argc, char* argv[]){
    if (argc < 2){
        printf("File name required.\n");
        return 1;
    }
    
    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL){
        printf("Error: Cannot open file.\n");
        return 1;
    }

    char line[256];
    OrderBook book = {NULL, NULL};
    int current_time = 0;
    int status = 0;

    while (fgets(line, sizeof(line), fp)){
   
        // Check for lines that exceed the buffer size
        // Then clear the rest of the line to prevent it from being read as the next command
        if (strchr(line, '\n') == NULL && !feof(fp)) {
            printf("Error: Invalid command at %s\n", line);

            int ch;
            // So then you read through every character and discard until you reach new line (\n) or EOF
            while ((ch = fgetc(fp)) != '\n' && ch != EOF) {
            }
            continue;
        }

        // ADD
        if (strncmp(line, "ADD", 3) == 0){

            Order *o = malloc(sizeof(Order));
            if (o == NULL){
                printf("Error: memory allocaion failed.");
                status = 1;
                break;
            }

            int n = parse_ADD(line, o);

            if (n == 0){
                printf("Error: Invalid command at %s", line);
                free(o);
                continue;
            }
            else if (!validate_ADD(o, &book, &current_time)){
                printf("Error: Invalid command at %s", line);
                free(o);
                continue;
            }
            else{
                if (o->type == BUY){
                    o->next = book.buy_head;
                    book.buy_head = o;
                    match_buy(&book, o);
                }
                else if (o->type == SELL){
                    o->next = book.sell_head;
                    book.sell_head = o;
                    match_sell(&book, o);
                }
            }
            }
        // CANCEL
        else if (strncmp(line, "CANCEL", 6) == 0){
            int id, time;
            int n = parse_CANCEL(line, &id, &time);
            if (!validate_CANCEL(id, time, n, &book, &current_time)){
                printf("Error: Invalid command at %s", line);
                continue;
            }
            else if (!cancel_order(&book, id)){
                printf("Error: Invalid command at %s", line);
                continue;
            }
            
        }
        // MODIFY
        else if (strncmp(line, "MODIFY", 6) == 0){
            Modify modify;
            if (!parse_MODIFY(line, &modify, &book, &current_time)){
                printf("Error: Invalid command at %s", line);
                continue;
            }
            else{
                Order* o = modify_order(&book, &modify);
                if (o == NULL){
                    printf("Error: Invalid command at %s", line);
                    continue;
                }
                else if (o->type == BUY){
                    match_buy(&book, o);
                }
                else if (o->type == SELL){
                    match_sell(&book, o);
                }
            }  
        }
        // QUERY
        else if (strncmp(line, "QUERY", 5) == 0){
            int time;

            if (!parse_QUERY(line, &time)){
                printf("Error: Invalid command at %s", line);
                continue;
            }

            // Time should not go backwards
            if (time < current_time){
                printf("Error: Invalid command at %s", line);
                continue;
            }

            current_time = time;
            expired_orders(&book, current_time);
            command_query(&book);

        }
        // For invalid commands
        else{
            printf("Error: Invalid command at %s", line);
            continue;
        }
    }
    free_list(book.buy_head);
    free_list(book.sell_head);
    fclose(fp);
    return status;
}

// Parse ADD command and store inside an Order struct
int parse_ADD(const char* line, Order *o){

    char type[5];
    char extra;
    int n = sscanf(line, "ADD id=%d %7s %4s pr=%lf qty=%d time=%d ttl=%d %c", &o->id, o->symbol, type, &o->price, &o->quantity, &o->time, &o->ttl, &extra);
    
    if (n != 7) return 0;

    if (strcmp(type, "BUY") == 0) o->type = BUY;
    else if (strcmp(type, "SELL") == 0) o->type = SELL;
    else return 0;

    return n;
}

// Validate a new order before adding it to the order book and updating system time.
int validate_ADD(const Order *o, OrderBook *book, int *current_time){

    // Reject invalid BUY or SELL
    if (o->type != BUY && o->type != SELL) return 0;

    // As per the prompt, these member variables should not be negative. Time is allowed to start a zero, however. 
    // The prompt does not specify if ID can be zero or negative, but it is safer to assume that it cannot.
    if (o->id <= 0) return 0;
    if (o->price <= 0) return 0;
    if (o->quantity <= 0) return 0;
    if (o->time < 0) return 0;
    if (o->ttl <= 0) return 0;

    // Before checking for duplicate IDs, it should update the time and removed expired orders first.
    if (o->time < *current_time) return 0;
    *current_time = o->time;
    expired_orders(book, *current_time);

    // Check for duplicate (only unique IDs)
    Order *curr = book->buy_head;
    while (curr != NULL){
        if (curr->id == o->id){
            return 0;
        }
        curr = curr->next;
    }

    curr = book->sell_head;
    while (curr != NULL){
        if (curr->id == o->id){
            return 0;
        }
        curr = curr->next;
    }

    // Validate the stock symbol last.
    // If there is a match, then validation is complete. 
    const char *symbols[] = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};
    int count = sizeof(symbols) / sizeof(symbols[0]);
    for (int i = 0; i < count; i++) {
        if (strcmp(o->symbol, symbols[i]) == 0) {
            return 1;
        }
    }
    
    return 0;
}

int parse_CANCEL(const char* line, int *id, int *time){
    char extra;
    int n = sscanf(line, "CANCEL id=%d time=%d %c", id, time, &extra);
    return n;
}

// Validate parsed CANCEL command and update system time. 
int validate_CANCEL(int id, int time, int n, OrderBook *book, int *current_time){
    if (n != 2) return 0;
    if (time < 0) return 0;
    if (id <= 0) return 0;
    if (time < *current_time) return 0;
    *current_time = time;
    expired_orders(book, *current_time);
    return 1;
}

int cancel_order(OrderBook *book, int id){
    if (remove_from_list(&book->buy_head, id)) return 1;
    if (remove_from_list(&book->sell_head, id)) return 1;
    return 0;
}

// Removes an order by ID from a singly linked list and free its memory.
// Used for cancelling and for matching when qty = 0.
int remove_from_list(Order **head, int id){
    Order *curr = *head;
    Order *prev = NULL;

    while (curr != NULL){
        if (curr->id == id){
            if (prev == NULL){
                *head = curr->next;
            }
            else{
                prev->next = curr->next;
            }
            free(curr);
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }
    return 0;
}

// Frees any linked list passed through.
void free_list(Order *head){
    Order *curr = head;
    while (curr != NULL){
        Order *next = curr->next;
        free(curr);
        curr = next;
    }
}

// Parse AND validate a MODIFY command. Update system time as well. 
int parse_MODIFY(const char* line, Modify *modify, OrderBook *book, int *current_time){
    char extra;
    int n = sscanf(line, "MODIFY id=%d pr=%lf qty=%d time=%d %c", &modify->id, &modify->price, &modify->quantity, &modify->time, &extra);
    
    // Validate the parsed MODIFY command.
    if (n != 4) return 0;
    if (modify->id <= 0) return 0;
    if (modify->quantity <= 0) return 0;
    if (modify->price <= 0) return 0;
    if (modify->time < 0) return 0;

    if (modify->time < *current_time) return 0;
    *current_time = modify->time;
    expired_orders(book, *current_time);
    return n;
}

// Match a BUY order with the lowest-price eligible SELL order.
void match_buy(OrderBook *book, Order *buy){
    while (buy->quantity > 0){
        Order *curr = book->sell_head;
        Order *min = NULL;
        while (curr != NULL){
            if (strcmp(curr->symbol, buy->symbol) == 0 && curr->price <= buy->price){
                if (min == NULL || curr->price < min->price){
                    min = curr;
                }
                else if(curr->price == min->price){
                    if (curr->time < min->time){
                        min = curr;
                    }
                }
            }
            curr = curr->next;
        }
        if (min == NULL){
            break;
        }
        int match = (buy->quantity < min->quantity) ? buy->quantity : min->quantity;
        printf("Matched: Order ID:%d Order ID:%d Quantity: %d\n", buy->id, min->id, match);
        buy->quantity -= match;
        min->quantity -= match;
        if (buy->quantity == 0 && min->quantity == 0) {
            remove_from_list(&book->sell_head, min->id);
            remove_from_list(&book->buy_head, buy->id);
            break;
        }
        if (buy->quantity == 0){
            remove_from_list(&book->buy_head, buy->id);
            break;
        }
        if (min->quantity == 0){
            remove_from_list(&book->sell_head, min->id);
        }
    }
}

// Match a SELL order with the highest-price eligible BUY order.
void match_sell(OrderBook *book, Order *sell){
    while (sell->quantity > 0){
        Order *curr = book->buy_head;
        Order *max = NULL;
        while (curr != NULL){
            if (strcmp(curr->symbol, sell->symbol) == 0 && curr->price >= sell->price){
                if (max == NULL || curr->price > max->price){
                    max = curr;
                }
                else if(curr->price == max->price){
                    if (curr->time < max->time){
                        max = curr;
                    }
                }
            }
            curr = curr->next;
        }
        if (max == NULL){
            break;
        }
        int match = (sell->quantity < max->quantity) ? sell->quantity : max->quantity;
        printf("Matched: Order ID:%d Order ID:%d Quantity: %d\n", max->id, sell->id, match);
        sell->quantity -= match;
        max->quantity -= match;

        if (sell->quantity == 0 && max->quantity == 0) {
            remove_from_list(&book->buy_head, max->id);
            remove_from_list(&book->sell_head, sell->id);
            break;
        }
        if (sell->quantity == 0){
            remove_from_list(&book->sell_head, sell->id);
            break;
        }
        if (max->quantity == 0){
            remove_from_list(&book->buy_head, max->id);
        }
    }
}

// Once an order is modified, it needs to start matching with that same modified order. 
// Return the order so it can match without traversing the entire list again. 
Order* modify_order(OrderBook *book, Modify *modify){
    Order *curr = book->buy_head;
    while (curr != NULL){
        if (curr->id == modify->id){
            curr->price = modify->price;
            curr->quantity = modify->quantity;
            curr->time = modify->time;
            return curr;
        }
        curr = curr->next;
    }
    curr = book->sell_head;
    while (curr != NULL){
        if (curr->id == modify->id){
            curr->price = modify->price;
            curr->quantity = modify->quantity;
            curr->time = modify->time;
            return curr;
        }
        curr = curr->next;
    }
    return NULL;    
}

// Expire orders before processing next command. 
// Should run after a parsed line/command is validated. 
void expired_orders(OrderBook *book, int current_time){
    Order* curr = book->buy_head;
    while (curr != NULL){
        Order* temp = curr->next;
        // long-long cast to prevent potential overflow when adding time and ttl.
        if ((long long)curr->time + curr->ttl <= current_time){
            int temp_ID = curr->id;
            remove_from_list(&book->buy_head, curr->id);
            printf("Buy order %d expired.\n", temp_ID);
        }
        curr = temp;
    }

    curr = book->sell_head;
    while (curr != NULL){
        Order* temp = curr->next;
        // long-long cast to prevent potential overflow when adding time and ttl.
        if ((long long)curr->time + curr->ttl <= current_time){
            int temp_ID = curr->id;
            remove_from_list(&book->sell_head, curr->id);
            printf("Sell order %d expired.\n", temp_ID);
        }
        curr = temp;
    }
}

int parse_QUERY(const char *line, int *time){
    char extra;
    int n = sscanf(line, "QUERY time=%d %c", time, &extra);
    return n == 1;
}

// Print all active orders in the order book at the time of the QUERY command
void command_query(OrderBook* book){
    const char *symbols[] = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};
    int count = sizeof(symbols) / sizeof(symbols[0]);

    for (int i = 0; i < count; i++) {
        const char *sym = symbols[i];
        Order *curr;
        int has_buy = 0;
        int has_sell = 0;

        printf("%s:\n", sym);

        printf("BUY orders:");
        curr = book->buy_head;
        while (curr != NULL) {
            if (strcmp(curr->symbol, sym) == 0) {
                printf(" id=%d pr=%.2f qty=%d time=%d ttl=%d",
                       curr->id, curr->price, curr->quantity, curr->time, curr->ttl);
                has_buy = 1;
            }
            curr = curr->next;
        }
        if (!has_buy) {
            printf(" none");
        }
        printf("\n");

        printf("SELL orders:");
        curr = book->sell_head;
        while (curr != NULL) {
            if (strcmp(curr->symbol, sym) == 0) {
                printf(" id=%d pr=%.2f qty=%d time=%d ttl=%d",
                       curr->id, curr->price, curr->quantity, curr->time, curr->ttl);
                has_sell = 1;
            }
            curr = curr->next;
        }
        if (!has_sell) {
            printf(" none");
        }
        printf("\n\n");
    }
}




