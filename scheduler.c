//
// Created by Manav Gharat on 2026-09-25.
// University Timetable Conflict Optimizer
// DS + DBMS Mini Project
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_NAME "timetableminiproject"
#define DB_PORT 3306

#define MAX_SESSIONS 50
#define MAX_SLOTS 15
#define NAME_SIZE 100


// DATA STRUCTURES

typedef struct {

    int slot_id;
    char day_of_week[20];
    char start_time[20];
    char end_time[20];

} TimeSlot;


typedef struct MakeupRequest {

    int request_id;
    int faculty_id;
    int session_id;
    int priority;

    struct MakeupRequest *next;

} MakeupRequest;

typedef struct {

    int session_id;
    int course_id;
    int faculty_id;
    int panel_id;
    int room_id;
    int slot_id;

    char course_name[NAME_SIZE];
    char faculty_name[NAME_SIZE];
    char panel_name[NAME_SIZE];
    char room_number[NAME_SIZE];

    char day_of_week[20];
    char start_time[20];
    char end_time[20];
    char session_type[20];

} Session;


// Graph
typedef struct {

    int vertices;

    int adjacency[MAX_SESSIONS][MAX_SESSIONS];

} Graph;


// A Linked List to store generated timetable sessions dynamically

typedef struct TimetableNode {
    Session session;
    struct TimetableNode *next;
} TimetableNode;

TimetableNode *createNode(Session session) {
    TimetableNode *node =
        (TimetableNode *)malloc(sizeof(TimetableNode));

    if (node == NULL) {
        printf("Memory allocation failed.\n");
        exit(1);
    }

    node->session = session;
    node->next = NULL;

    return node;
}

// MYSQL CONNECTION

MYSQL *connectDatabase() {

    MYSQL *conn;

    conn = mysql_init(NULL);

    if (conn == NULL) {

        printf("MySQL initialization failed.\n");
        exit(1);
    }
    char password[128];
    printf("Enter Password: ");
    scanf("%127s", password);

    if (mysql_real_connect(
            conn,
            DB_HOST,
            DB_USER,
            password,
            DB_NAME,
            DB_PORT,
            NULL,
            0) == NULL) {

        printf("Database connection failed:\n");
        printf("%s\n", mysql_error(conn));

        mysql_close(conn);

        exit(1);
    }

    printf("========================================\n");
    printf("Connected to MySQL successfully!\n");
    printf("========================================\n\n");

    return conn;
}

// LOAD TIME SLOTS FROM MYSQL

int loadTimeSlots(MYSQL *conn, TimeSlot slots[]) {

    const char *query =
        "SELECT slot_id, day_of_week, start_time, end_time "
        "FROM time_slots "
        "ORDER BY slot_id;";

    if (mysql_query(conn, query) != 0) {
        printf("Failed to load time slots:\n");
        printf("%s\n", mysql_error(conn));
        return 0;
    }

    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL) {
        printf("Failed to retrieve time slots:\n");
        printf("%s\n", mysql_error(conn));
        return 0;
    }

    MYSQL_ROW row;

    int count = 0;

    while ((row = mysql_fetch_row(result)) != NULL &&
           count < MAX_SLOTS) {

        slots[count].slot_id = atoi(row[0]);

        strncpy(
            slots[count].day_of_week,
            row[1],
            sizeof(slots[count].day_of_week) - 1
        );

        slots[count].day_of_week[
            sizeof(slots[count].day_of_week) - 1
        ] = '\0';

        strncpy(
            slots[count].start_time,
            row[2],
            sizeof(slots[count].start_time) - 1
        );

        slots[count].start_time[
            sizeof(slots[count].start_time) - 1
        ] = '\0';

        strncpy(
            slots[count].end_time,
            row[3],
            sizeof(slots[count].end_time) - 1
        );

        slots[count].end_time[
            sizeof(slots[count].end_time) - 1
        ] = '\0';

        count++;
           }

    mysql_free_result(result);

    return count;
}

// LOAD SESSIONS FROM MYSQL

int loadSessions(MYSQL *conn, Session sessions[]) {

    const char *query =

        "SELECT "
        "cs.session_id, "
        "cs.course_id, "
        "cs.faculty_id, "
        "cs.panel_id, "
        "cs.room_id, "
        "cs.slot_id, "
        "c.course_name, "
        "f.name, "
        "p.panel_name, "
        "r.room_number, "
        "ts.day_of_week, "
        "ts.start_time, "
        "ts.end_time, "
        "cs.session_type "

        "FROM class_sessions cs "

        "JOIN courses c "
        "ON cs.course_id = c.course_id "

        "JOIN faculty f "
        "ON cs.faculty_id = f.faculty_id "

        "JOIN panels p "
        "ON cs.panel_id = p.panel_id "

        "JOIN rooms r "
        "ON cs.room_id = r.room_id "

        "JOIN time_slots ts "
        "ON cs.slot_id = ts.slot_id "

        "WHERE cs.status = 'SCHEDULED' "

        "ORDER BY cs.session_id;";


    if (mysql_query(conn, query) != 0) {

        printf("Failed to load sessions:\n");
        printf("%s\n", mysql_error(conn));

        return 0;
    }


    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL) {

        printf("Failed to retrieve result:\n");
        printf("%s\n", mysql_error(conn));

        return 0;
    }


    MYSQL_ROW row;

    int count = 0;


    while ((row = mysql_fetch_row(result)) != NULL) {

        if (count >= MAX_SESSIONS) {

            break;
        }


        sessions[count].session_id = atoi(row[0]);
        sessions[count].course_id = atoi(row[1]);
        sessions[count].faculty_id = atoi(row[2]);
        sessions[count].panel_id = atoi(row[3]);
        sessions[count].room_id = atoi(row[4]);
        sessions[count].slot_id = atoi(row[5]);


        strncpy(
            sessions[count].course_name,
            row[6],
            NAME_SIZE - 1
        );

        sessions[count].course_name[NAME_SIZE - 1] = '\0';


        strncpy(
            sessions[count].faculty_name,
            row[7],
            NAME_SIZE - 1
        );

        sessions[count].faculty_name[NAME_SIZE - 1] = '\0';


        strncpy(
            sessions[count].panel_name,
            row[8],
            NAME_SIZE - 1
        );

        sessions[count].panel_name[NAME_SIZE - 1] = '\0';


        strncpy(
            sessions[count].room_number,
            row[9],
            NAME_SIZE - 1
        );

        sessions[count].room_number[NAME_SIZE - 1] = '\0';


        strncpy(
            sessions[count].day_of_week,
            row[10],
            sizeof(sessions[count].day_of_week) - 1
        );

        sessions[count].day_of_week[
            sizeof(sessions[count].day_of_week) - 1
        ] = '\0';


        strncpy(
            sessions[count].start_time,
            row[11],
            sizeof(sessions[count].start_time) - 1
        );

        sessions[count].start_time[
            sizeof(sessions[count].start_time) - 1
        ] = '\0';


        strncpy(
            sessions[count].end_time,
            row[12],
            sizeof(sessions[count].end_time) - 1
        );

        sessions[count].end_time[
            sizeof(sessions[count].end_time) - 1
        ] = '\0';

        strncpy(
            sessions[count].session_type,
            row[13],
            sizeof(sessions[count].session_type) - 1
        );

        sessions[count].session_type[
            sizeof(sessions[count].session_type) - 1
        ] = '\0';

        count++;
    }


    mysql_free_result(result);

    return count;
}


// DISPLAY DATA LOADED FROM MYSQL

void displaySessions(Session sessions[], int count) {

    printf("\n");
    printf("========================================\n");
    printf("        DATA LOADED FROM MYSQL\n");
    printf("========================================\n");


    for (int i = 0; i < count; i++) {

        printf("\nSession ID : %d\n",
               sessions[i].session_id);

        printf("Course     : %s\n",
               sessions[i].course_name);

        printf("Faculty    : %s\n",
               sessions[i].faculty_name);

        printf("Panel      : %s\n",
               sessions[i].panel_name);

        printf("Room       : %s\n",
               sessions[i].room_number);

        printf("Time       : %s %s - %s\n",
               sessions[i].day_of_week,
               sessions[i].start_time,
               sessions[i].end_time);
    }
}


// GRAPH INITIALIZATION

void initGraph(Graph *graph, int vertices) {

    graph->vertices = vertices;


    for (int i = 0; i < vertices; i++) {

        for (int j = 0; j < vertices; j++) {

            graph->adjacency[i][j] = 0;
        }
    }
}



// ADD EDGE

void addConflict(Graph *graph, int a, int b) {

    graph->adjacency[a][b] = 1;
    graph->adjacency[b][a] = 1;
}


// BUILD CONFLICT GRAPH

void buildConflictGraph(
    Graph *graph,
    Session sessions[],
    int count
) {

    initGraph(graph, count);


    for (int i = 0; i < count; i++) {

        for (int j = i + 1; j < count; j++) {


            /*
             * CONFLICT 1:
             *
             * Same panel
             *
             * Students belonging to the same panel
             * should not have two classes at the same time.
             */

            if (sessions[i].panel_id ==
                sessions[j].panel_id) {

                addConflict(graph, i, j);

                continue;
            }


            /*
             * CONFLICT 2:
             *
             * Same faculty
             *
             * A teacher cannot teach two classes
             * at the same time.
             */

            if (sessions[i].faculty_id ==
                sessions[j].faculty_id) {

                addConflict(graph, i, j);
            }
        }
    }
}


// DISPLAY CONFLICT GRAPH

void displayGraph(
    Graph *graph,
    Session sessions[]
) {

    printf("\n");
    printf("========================================\n");
    printf("             CONFLICT GRAPH\n");
    printf("========================================\n");


    for (int i = 0; i < graph->vertices; i++) {

        printf("\n%s -> ",
               sessions[i].course_name);


        for (int j = 0; j < graph->vertices; j++) {

            if (graph->adjacency[i][j] == 1) {

                printf("%s, ",
                       sessions[j].course_name);
            }
        }
    }

    printf("\n");
}


// CHECK IF SLOT IS SAFE


int isSafe(
    Graph *graph,
    int course,
    int slot,
    int color[]
) {

    for (int i = 0; i < graph->vertices; i++) {

        /*
         * If there is a conflict AND
         * the conflicting course already
         * has the same slot...
         */

        if (graph->adjacency[course][i] == 1 &&
            color[i] == slot) {

            return 0;
        }
    }

    return 1;
}


// GRAPH COLORING USING BACKTRACKING

int graphColoring(
    Graph *graph,
    int color[],
    int totalSlots,
    int course
)
{
    // All courses assigned
    if (course == graph->vertices)
    {
        return 1;
    }

    // If this class already has a fixed slot,
    // keep that slot and move to the next class.
    if (color[course] != -1)
    {
        if (!isSafe(
                graph,
                course,
                color[course],
                color))
        {
            return 0;
        }

        return graphColoring(
            graph,
            color,
            totalSlots,
            course + 1
        );
    }

    // Otherwise try available slots
    for (int slot = 0; slot < totalSlots; slot++)
    {
        if (isSafe(
                graph,
                course,
                slot,
                color))
        {
            color[course] = slot;

            if (graphColoring(
                    graph,
                    color,
                    totalSlots,
                    course + 1))
            {
                return 1;
            }

            // Backtrack
            color[course] = -1;
        }
    }

    return 0;
}


int findSlotIndex(TimeSlot slots[], int slotCount, int slot_id)
{
    for (int i = 0; i < slotCount; i++)
    {
        if (slots[i].slot_id == slot_id)
        {
            return i;
        }
    }

    return -1;
}

// DISPLAY GENERATED TIMETABLE

void displayTimetable(
    Session sessions[],
    TimeSlot slots[],
    int color[],
    int count
) {

    printf("\n");
    printf("========================================\n");
    printf("        GENERATED TIMETABLE\n");
    printf("========================================\n");

    for (int i = 0; i < count; i++) {

        int slotIndex = color[i];

        printf("\n");

        printf("Course  : %s\n",
               sessions[i].course_name);

        printf("Faculty : %s\n",
               sessions[i].faculty_name);

        printf("Panel   : %s\n",
               sessions[i].panel_name);

        printf("Type    : %s\n",
                sessions[i].session_type);

        printf("Time    : %s %s - %s\n",
               slots[slotIndex].day_of_week,
               slots[slotIndex].start_time,
               slots[slotIndex].end_time);
    }
}


// If a teacher accepts a Class

void acceptAvailableClass(
    MYSQL *conn,
    int notification_id,
    int faculty_id,
    int session_id
) {

    char query[700];

    /*
     * Make sure this teacher actually owns
     * the notification.
     */

    snprintf(
        query,
        sizeof(query),

        "SELECT notification_id "
        "FROM notifications "
        "WHERE notification_id = %d "
        "AND faculty_id = %d "
        "AND session_id = %d "
        "AND notification_type = 'AVAILABLE_SLOT' "
        "AND response_status = 'PENDING'",

        notification_id,
        faculty_id,
        session_id
    );

    if (mysql_query(conn, query) != 0) {
        printf("Database error: %s\n",
               mysql_error(conn));
        return;
    }

    MYSQL_RES *result =
        mysql_store_result(conn);

    if (mysql_num_rows(result) == 0) {

        printf(
            "\nThis notification is no longer available.\n"
        );

        mysql_free_result(result);
        return;
    }

    mysql_free_result(result);


    /*
     * Assign the teacher.
     */

    snprintf(
        query,
        sizeof(query),

        "UPDATE class_sessions "
        "SET faculty_id = %d, "
        "status = 'SCHEDULED' "
        "WHERE session_id = %d "
        "AND status = 'CANCELLED'",

        faculty_id,
        session_id
    );

    if (mysql_query(conn, query) != 0) {

        printf(
            "Failed to accept class: %s\n",
            mysql_error(conn)
        );

        return;
    }

    if (mysql_affected_rows(conn) == 0) {

        printf(
            "\nAnother teacher may have already "
            "accepted this class.\n"
        );

        return;
    }


    /*
     * Mark this notification accepted.
     */

    snprintf(
        query,
        sizeof(query),

        "UPDATE notifications "
        "SET response_status = 'ACCEPTED', "
        "is_read = 1 "
        "WHERE notification_id = %d",

        notification_id
    );

    mysql_query(conn, query);


    /*
     * Remove notifications from
     * the other teachers.
     */

    snprintf(
        query,
        sizeof(query),

        "DELETE FROM notifications "
        "WHERE session_id = %d "
        "AND notification_type = 'AVAILABLE_SLOT' "
        "AND notification_id <> %d",

        session_id,
        notification_id
    );

    mysql_query(conn, query);


    printf(
        "\n========================================\n"
        "       CLASS ACCEPTED SUCCESSFULLY\n"
        "========================================\n"
    );
}



// Clear the notification table when a class is updated

void clearSlotNotifications(MYSQL *conn, int session_id) {

    char query[256];

    snprintf(
        query,
        sizeof(query),
        "DELETE FROM notifications "
        "WHERE session_id = %d "
        "AND notification_type = 'AVAILABLE_SLOT'",
        session_id
    );

    if (mysql_query(conn, query) != 0) {

        printf("Failed to clear notifications: %s\n",
               mysql_error(conn));

        return;
    }

    printf("Old slot notifications cleared.\n");
}

// View notifications from the notifications table

void viewNotifications(
    MYSQL *conn,
    int faculty_id
) {

    char query[600];

    snprintf(
        query,
        sizeof(query),

        "SELECT "
        "notification_id, "
        "session_id, "
        "message, "
        "response_status, "
        "is_read "
        "FROM notifications "
        "WHERE faculty_id = %d "
        "ORDER BY created_at DESC",

        faculty_id
    );

    if (mysql_query(conn, query) != 0) {
        printf(
            "Failed to load notifications: %s\n",
            mysql_error(conn)
        );
        return;
    }

    MYSQL_RES *result =
        mysql_store_result(conn);

    MYSQL_ROW row;

    printf("\n");
    printf("========================================\n");
    printf("          MY NOTIFICATIONS\n");
    printf("========================================\n");

    while ((row = mysql_fetch_row(result)) != NULL) {

        printf("\nNotification ID : %s\n", row[0]);
        printf("Session ID      : %s\n", row[1]);
        printf("Message         : %s\n", row[2]);
        printf("Status          : %s\n", row[3]);
        printf("Read            : %s\n",
               atoi(row[4]) ? "Yes" : "No");
    }

    mysql_free_result(result);
}


// Notify the relevant teachers about the canceled classes

void createAvailableSlotNotifications(
    MYSQL *conn,
    int released_session_id
)
{
    char query[2000];

    /*
     * Remove old notifications for this released slot.
     */
    snprintf(
        query,
        sizeof(query),
        "DELETE FROM notifications "
        "WHERE session_id = %d "
        "AND notification_type = 'AVAILABLE_SLOT'",
        released_session_id
    );

    if (mysql_query(conn, query) != 0)
    {
        printf(
            "Failed to clear old notifications: %s\n",
            mysql_error(conn)
        );
        return;
    }


    /*
     * Create notifications only for:
     *
     * 1. Pending makeup requests
     * 2. Faculty assigned to the affected panel
     * 3. Faculty who are not the original teacher
     */
    snprintf(
        query,
        sizeof(query),

        "INSERT INTO notifications "
        "(faculty_id, session_id, makeup_request_id, "
        "available_room_id, available_slot_id, "
        "message, notification_type, response_status) "

        "SELECT "
        "mr.faculty_id, "
        "released.session_id, "
        "mr.request_id, "
        "released.room_id, "
        "released.slot_id, "

        "CONCAT("
        "'Makeup slot available for ', "
        "c.course_name, ': ', "
        "ts.day_of_week, ' ', "
        "ts.start_time, '-', ts.end_time, "
        "', Room ', r.room_number"
        "), "

        "'AVAILABLE_SLOT', "
        "'PENDING' "

        "FROM makeup_requests mr "

        "JOIN class_sessions original "
        "ON mr.session_id = original.session_id "

        "JOIN courses c "
        "ON original.course_id = c.course_id "

        "JOIN class_sessions released "
        "ON released.session_id = %d "

        "JOIN panels p "
        "ON released.panel_id = p.panel_id "

        "JOIN time_slots ts "
        "ON released.slot_id = ts.slot_id "

        "JOIN rooms r "
        "ON released.room_id = r.room_id "

        "JOIN faculty_assignments fa "
        "ON fa.faculty_id = mr.faculty_id "
        "AND fa.panel_id = released.panel_id "

        "WHERE mr.status = 'PENDING' "

        "AND released.status = 'CANCELLED' "

        "AND mr.faculty_id <> released.faculty_id "

        "AND NOT EXISTS ("
        "SELECT 1 "
        "FROM notifications n "
        "WHERE n.faculty_id = mr.faculty_id "
        "AND n.session_id = released.session_id "
        "AND n.makeup_request_id = mr.request_id "
        "AND n.notification_type = 'AVAILABLE_SLOT' "
        "AND n.response_status = 'PENDING'"
        ")",

        released_session_id
    );


    if (mysql_query(conn, query) != 0)
    {
        printf(
            "Failed to create notifications: %s\n",
            mysql_error(conn)
        );
        return;
    }


    printf(
        "%llu makeup notification(s) created.\n",
        mysql_affected_rows(conn)
    );
}

// Add classes canceled to the table

void cancelClass(MYSQL *conn, int faculty_id)
{
    int session_id;

    printf("\n========================================\n");
    printf("             CANCEL CLASS\n");
    printf("========================================\n");

    printf("\nEnter Session ID to cancel: ");
    scanf("%d", &session_id);


    char query[512];

    snprintf(
        query,
        sizeof(query),

        "UPDATE class_sessions "
        "SET status = 'CANCELLED' "
        "WHERE session_id = %d "
        "AND faculty_id = %d "
        "AND status = 'SCHEDULED'",

        session_id,
        faculty_id
    );


    if (mysql_query(conn, query) != 0)
    {
        printf(
            "Failed to cancel class: %s\n",
            mysql_error(conn)
        );

        return;
    }


    if (mysql_affected_rows(conn) == 0)
    {
        printf(
            "\nClass not found, not assigned to you, "
            "or already cancelled.\n"
        );

        return;
    }


    printf(
        "\nClass cancelled successfully!\n"
    );


    // Create notifications for other teachers

    createAvailableSlotNotifications(
        conn,
        session_id
    );
}

void generateTimetable(MYSQL *conn);
void showNotifications(MYSQL *conn, int faculty_id);
void respondToNotification(MYSQL *conn, int notification_id, int faculty_id);

void showNotifications(MYSQL *conn, int faculty_id)
{
    char query[1000];

    snprintf(query, sizeof(query),
        "SELECT n.notification_id, n.session_id, n.message, "
        "n.response_status "
        "FROM notifications n "
        "WHERE n.faculty_id = %d "
        "AND n.response_status = 'PENDING' "
        "ORDER BY n.notification_id",
        faculty_id);

    if (mysql_query(conn, query) != 0)
    {
        printf("Failed to retrieve notifications: %s\n",
               mysql_error(conn));
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL)
    {
        printf("Failed to retrieve results.\n");
        return;
    }

    MYSQL_ROW row;

    if (mysql_num_rows(result) == 0)
    {
        printf("\nNo pending notifications.\n");
        mysql_free_result(result);
        return;
    }

    printf("\n========================================\n");
    printf("          YOUR NOTIFICATIONS\n");
    printf("========================================\n");

    while ((row = mysql_fetch_row(result)) != NULL)
    {
        printf("\nNotification ID : %s\n", row[0]);
        printf("Session ID      : %s\n", row[1]);
        printf("Message         : %s\n", row[2]);
        printf("Status          : %s\n", row[3]);

        printf("\n1. Accept Class");
        printf("\n2. Decline");
        printf("\n0. Back\n");

        int choice;
        printf("Enter choice: ");
        scanf("%d", &choice);

        if (choice == 1)
        {
            respondToNotification(conn, atoi(row[0]), faculty_id);
        }
        else if (choice == 2)
        {
            char updateQuery[500];

            snprintf(updateQuery, sizeof(updateQuery),
                "UPDATE notifications "
                "SET response_status = 'DECLINED', is_read = 1 "
                "WHERE notification_id = %d "
                "AND faculty_id = %d "
                "AND response_status = 'PENDING'",
                atoi(row[0]), faculty_id);

            if (mysql_query(conn, updateQuery) == 0)
            {
                printf("\nClass declined.\n");
            }
            else
            {
                printf("\nFailed to decline: %s\n",
                       mysql_error(conn));
            }
        }
        else if (choice == 0)
        {
            break;
        }
        else
        {
            printf("\nInvalid choice.\n");
        }
    }

    mysql_free_result(result);
}

void respondToNotification(
    MYSQL *conn,
    int notification_id,
    int faculty_id
)
{
    char query[2000];


    // Get notification details

    snprintf(
        query,
        sizeof(query),

        "SELECT "
        "session_id, "
        "makeup_request_id, "
        "available_room_id, "
        "available_slot_id "

        "FROM notifications "

        "WHERE notification_id = %d "
        "AND faculty_id = %d "
        "AND notification_type = 'AVAILABLE_SLOT' "
        "AND response_status = 'PENDING'",

        notification_id,
        faculty_id
    );


    if (mysql_query(conn, query) != 0)
    {
        printf(
            "Failed to find notification: %s\n",
            mysql_error(conn)
        );
        return;
    }


    MYSQL_RES *result =
        mysql_store_result(conn);


    if (result == NULL)
    {
        printf("Database error.\n");
        return;
    }


    MYSQL_ROW row =
        mysql_fetch_row(result);


    if (row == NULL)
    {
        printf(
            "\nNotification is no longer available.\n"
        );

        mysql_free_result(result);
        return;
    }


    int released_session_id = atoi(row[0]);
    int makeup_request_id = atoi(row[1]);
    int room_id = atoi(row[2]);
    int slot_id = atoi(row[3]);


    mysql_free_result(result);


    // Make sure the released class is still cancelled


    snprintf(
        query,
        sizeof(query),

        "SELECT status "
        "FROM class_sessions "
        "WHERE session_id = %d",

        released_session_id
    );


    if (mysql_query(conn, query) != 0)
    {
        printf(
            "Failed to check released class: %s\n",
            mysql_error(conn)
        );
        return;
    }


    result = mysql_store_result(conn);

    row = mysql_fetch_row(result);


    if (row == NULL ||
        strcmp(row[0], "CANCELLED") != 0)
    {
        printf(
            "\nThis slot is no longer available.\n"
        );

        mysql_free_result(result);
        return;
    }


    mysql_free_result(result);


    // Check room conflict

    snprintf(
        query,
        sizeof(query),

        "SELECT COUNT(*) "
        "FROM class_sessions "
        "WHERE room_id = %d "
        "AND slot_id = %d "
        "AND status = 'SCHEDULED'",

        room_id,
        slot_id
    );


    if (mysql_query(conn, query) != 0)
    {
        printf("Room conflict check failed.\n");
        return;
    }


    result = mysql_store_result(conn);
    row = mysql_fetch_row(result);

    int roomConflict = atoi(row[0]);

    mysql_free_result(result);


    if (roomConflict > 0)
    {
        printf(
            "\nThe room is no longer available.\n"
        );
        return;
    }


    // Check faculty conflict

    snprintf(
        query,
        sizeof(query),

        "SELECT COUNT(*) "
        "FROM class_sessions "
        "WHERE faculty_id = %d "
        "AND slot_id = %d "
        "AND status = 'SCHEDULED'",

        faculty_id,
        slot_id
    );


    if (mysql_query(conn, query) != 0)
    {
        printf("Faculty conflict check failed.\n");
        return;
    }


    result = mysql_store_result(conn);
    row = mysql_fetch_row(result);

    int facultyConflict = atoi(row[0]);

    mysql_free_result(result);


    if (facultyConflict > 0)
    {
        printf(
            "\nYou already have a class during this slot.\n"
        );
        return;
    }


    // Create the makeup class

    snprintf(
        query,
        sizeof(query),

        "INSERT INTO class_sessions "
        "(course_id, faculty_id, panel_id, room_id, slot_id, status, session_type) "

        "SELECT "
        "cs.course_id, "
        "%d, "
        "cs.panel_id, "
        "%d, "
        "%d, "
        "'SCHEDULED', "
        "'MAKEUP' "

        "FROM makeup_requests mr "

        "JOIN class_sessions cs "
        "ON mr.session_id = cs.session_id "

        "WHERE mr.request_id = %d "
        "AND mr.faculty_id = %d "
        "AND mr.status = 'PENDING'",

        faculty_id,
        room_id,
        slot_id,
        makeup_request_id,
        faculty_id
    );


    if (mysql_query(conn, query) != 0)
    {
        printf(
            "Failed to create makeup class: %s\n",
            mysql_error(conn)
        );
        return;
    }


    if (mysql_affected_rows(conn) == 0)
    {
        printf(
            "\nMakeup request is no longer pending.\n"
        );
        return;
    }


    // Mark makeup request as fulfilled

    snprintf(
        query,
        sizeof(query),

        "UPDATE makeup_requests "
        "SET status = 'FULFILLED' "
        "WHERE request_id = %d",

        makeup_request_id
    );

    mysql_query(conn, query);


    // Mark notification as accepted

    snprintf(
        query,
        sizeof(query),

        "UPDATE notifications "
        "SET response_status = 'ACCEPTED', "
        "is_read = 1 "
        "WHERE notification_id = %d",

        notification_id
    );

    mysql_query(conn, query);



    // Expire other teachers' notifications


    snprintf(
        query,
        sizeof(query),

        "UPDATE notifications "
        "SET response_status = 'EXPIRED', "
        "is_read = 1 "
        "WHERE session_id = %d "
        "AND makeup_request_id = %d "
        "AND notification_id <> %d "
        "AND response_status = 'PENDING'",

        released_session_id,
        makeup_request_id,
        notification_id
    );

    mysql_query(conn, query);


    printf(
        "\n========================================\n"
        "       MAKEUP CLASS ACCEPTED!\n"
        "========================================\n"
    );

    printf(
        "A new makeup class has been scheduled.\n"
    );

    printf(
        "Room ID : %d\n",
        room_id
    );

    printf(
        "Slot ID : %d\n",
        slot_id
    );
}
void generateTimetable(MYSQL *conn)
{
    TimeSlot slots[MAX_SLOTS];

    int slotCount = loadTimeSlots(conn, slots);

    if (slotCount == 0)
    {
        printf("\nNo time slots found.\n");
        return;
    }


    Session sessions[MAX_SESSIONS];

    int sessionCount = loadSessions(conn, sessions);

    if (sessionCount == 0)
    {
        printf("\nNo scheduled classes found.\n");
        return;
    }


    printf("\n%d scheduled classes loaded.\n",
           sessionCount);


    // Display classes loaded from MySQL

    displaySessions(
        sessions,
        sessionCount
    );


    // Build conflict graph

    Graph graph;

    buildConflictGraph(
        &graph,
        sessions,
        sessionCount
    );


    // Display conflict graph

    displayGraph(
        &graph,
        sessions
    );


    // Graph coloring

    int color[MAX_SESSIONS];

    for (int i = 0; i < sessionCount; i++)
    {
        color[i] = -1;

        // Preserve the slot of an existing makeup class
        if (strcmp(sessions[i].session_type, "MAKEUP") == 0)
        {
            int slotIndex = findSlotIndex(
                slots,
                slotCount,
                sessions[i].slot_id
            );

            if (slotIndex != -1)
            {
                color[i] = slotIndex;
            }
        }
    }


    // Use the actual number of slots
    int totalSlots = slotCount;


    if (graphColoring(
            &graph,
            color,
            totalSlots,
            0))
    {
        printf("\nTimetable generated successfully!\n");


        displayTimetable(
            sessions,
            slots,
            color,
            sessionCount
        );
    }
    else
    {
        printf("\n");
        printf("No valid timetable could be generated.\n");
    }
}

// MAIN

int main()
{
    // 1. Connect to MySQL

    MYSQL *conn = connectDatabase();

    if (conn == NULL)
    {
        return 1;
    }


    // 2. Select faculty for demo

    int current_user_id;

    printf("\n========================================\n");
    printf("       UNIVERSITY TIMETABLE SYSTEM\n");
    printf("========================================\n");

    printf("\nSelect Faculty:\n");
    printf("1. Prathm\n");
    printf("2. Vrukshita\n");
    printf("3. Shreya\n");
    printf("0. Exit\n");

    printf("\nEnter choice: ");
    scanf("%d", &current_user_id);

    if (current_user_id == 0)
    {
        mysql_close(conn);
        return 0;
    }


    // 3. Main Menu

    int choice;

    while (1)
    {
        printf("\n========================================\n");
        printf("       UNIVERSITY TIMETABLE SYSTEM\n");
        printf("========================================\n");

        printf("Logged in as Faculty ID: %d\n",
               current_user_id);

        printf("\n");
        printf("1. View Notifications\n");
        printf("2. Generate Timetable\n");
        printf("3. Cancel Class\n");
        printf("0. Exit\n");

        printf("\nEnter choice: ");
        scanf("%d", &choice);


        switch (choice)
        {
            case 1:
                showNotifications(conn, current_user_id);
                break;

            case 2:
                generateTimetable(conn);
                break;

            case 3:
                cancelClass(conn, current_user_id);
                break;

            case 0:
                printf("\nGoodbye!\n");

                mysql_close(conn);

                return 0;

            default:
                printf("\nInvalid choice. Please try again.\n");
        }
    }

    // This technically won't be reached because
    // case 0 closes the connection.

    mysql_close(conn);

    return 0;
}