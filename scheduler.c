//
// Created by Manav Gharat on 2026-09-25.
// University Timetable Conflict Optimizer
// DS + DBMS Mini Project
//

#include <stdio.h>
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
        "ts.end_time "

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
) {

    /*
     * Base case:
     *
     * Every course has been assigned
     * a time slot.
     */

    if (course == graph->vertices) {

        return 1;
    }


    /*
     * Try every available slot.
     */

    for (int slot = 0; slot < totalSlots; slot++) {


        /*
         * Check whether this slot is safe.
         */

        if (isSafe(
                graph,
                course,
                slot,
                color)) {


            /*
             * Assign slot.
             */

            color[course] = slot;


            /*
             * Try assigning a slot
             * to the next course.
             */

            if (graphColoring(
                    graph,
                    color,
                    totalSlots,
                    course + 1)) {

                return 1;
            }


            /*
             * BACKTRACK
             *
             * If the choice caused a conflict,
             * remove it and try another slot.
             */

            color[course] = -1;
        }
    }


    /*
     * No slot worked.
     */

    return 0;
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
    int session_id
) {

    char query[1500];

    snprintf(
        query,
        sizeof(query),

        "INSERT INTO notifications "
        "(faculty_id, session_id, message, "
        "notification_type, response_status) "

        "SELECT "
        "fa.faculty_id, "
        "%d, "

        "CONCAT("
        "'A class slot has become available: ', "
        "p.panel_name, ', ', "
        "ts.day_of_week, ' ', "
        "ts.start_time, '-', ts.end_time, "
        "', Room ', r.room_number"
        "), "

        "'AVAILABLE_SLOT', "
        "'PENDING' "

        "FROM class_sessions cs "

        "JOIN panels p "
        "ON cs.panel_id = p.panel_id "

        "JOIN time_slots ts "
        "ON cs.slot_id = ts.slot_id "

        "JOIN rooms r "
        "ON cs.room_id = r.room_id "

        "JOIN faculty_assignments fa "
        "ON fa.panel_id = cs.panel_id "

        "WHERE cs.session_id = %d "

        "AND fa.faculty_id <> cs.faculty_id "

        "AND NOT EXISTS ("
        "SELECT 1 "
        "FROM notifications n "
        "WHERE n.faculty_id = fa.faculty_id "
        "AND n.session_id = cs.session_id "
        "AND n.notification_type = 'AVAILABLE_SLOT'"
        "AND n.response_status = 'PENDING'"
        ")",

        session_id,
        session_id
    );

    if (mysql_query(conn, query) != 0) {

        printf(
            "Failed to create notifications: %s\n",
            mysql_error(conn)
        );

        return;
    }

    printf(
        "%llu teacher notification(s) created.\n",
        mysql_affected_rows(conn)
    );
}

// Add classes canceled to the table

void cancelClass(MYSQL *conn, int session_id) {

    char query[512];

    snprintf(
        query,
        sizeof(query),
        "UPDATE class_sessions "
        "SET status = 'CANCELLED' "
        "WHERE session_id = %d "
        "AND status = 'SCHEDULED'",
        session_id
    );

    if (mysql_query(conn, query) != 0) {
        printf("Failed to cancel class: %s\n",
               mysql_error(conn));
        return;
    }

    if (mysql_affected_rows(conn) == 0) {
        printf("Class not found or already cancelled.\n");
        return;
    }

    printf("\nClass cancelled successfully.\n");

    createAvailableSlotNotifications(conn, session_id);
}
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

void respondToNotification(MYSQL *conn, int notification_id, int faculty_id)
{
    char query[1000];

    /*
     * First find the session associated with this notification.
     */

    snprintf(query, sizeof(query),
        "SELECT session_id "
        "FROM notifications "
        "WHERE notification_id = %d "
        "AND faculty_id = %d "
        "AND response_status = 'PENDING'",
        notification_id, faculty_id);

    if (mysql_query(conn, query) != 0)
    {
        printf("Failed to find notification: %s\n",
               mysql_error(conn));
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL)
    {
        printf("Database error.\n");
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(result);

    if (row == NULL)
    {
        printf("\nThis notification is no longer available.\n");
        mysql_free_result(result);
        return;
    }

    int session_id = atoi(row[0]);

    mysql_free_result(result);


    /*
     * Update the class ONLY if it is still cancelled.
     *
     * This prevents two teachers from accepting
     * the same class.
     */

    snprintf(query, sizeof(query),
        "UPDATE class_sessions "
        "SET faculty_id = %d, status = 'SCHEDULED' "
        "WHERE session_id = %d "
        "AND status = 'CANCELLED'",
        faculty_id, session_id);

    if (mysql_query(conn, query) != 0)
    {
        printf("Failed to accept class: %s\n",
               mysql_error(conn));
        return;
    }

    if (mysql_affected_rows(conn) == 0)
    {
        printf("\nSorry! This class has already been taken.\n");
        return;
    }


    /*
     * Mark this teacher's notification as ACCEPTED.
     */

    snprintf(query, sizeof(query),
        "UPDATE notifications "
        "SET response_status = 'ACCEPTED', is_read = 1 "
        "WHERE notification_id = %d "
        "AND faculty_id = %d",
        notification_id, faculty_id);

    if (mysql_query(conn, query) != 0)
    {
        printf("Failed to update notification: %s\n",
               mysql_error(conn));
        return;
    }


    /*
     * Expire notifications belonging to
     * other teachers.
     */

    snprintf(query, sizeof(query),
        "UPDATE notifications "
        "SET response_status = 'EXPIRED', is_read = 1 "
        "WHERE session_id = %d "
        "AND faculty_id <> %d "
        "AND response_status = 'PENDING'",
        session_id, faculty_id);

    if (mysql_query(conn, query) != 0)
    {
        printf("Failed to expire other notifications: %s\n",
               mysql_error(conn));
        return;
    }

    printf("\n========================================\n");
    printf("       CLASS ACCEPTED SUCCESSFULLY!\n");
    printf("========================================\n");

    printf("Teacher ID : %d\n", faculty_id);
    printf("Session ID : %d\n", session_id);
    printf("Status     : SCHEDULED\n");
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
            // VIEW NOTIFICATIONS

            case 1:

                showNotifications(
                    conn,
                    current_user_id
                );

                break;


            // GENERATE TIMETABLE

            case 2:
            {
                // Load time slots

                TimeSlot slots[MAX_SLOTS];

                int slotCount =
                    loadTimeSlots(
                        conn,
                        slots
                    );


                if (slotCount == 0)
                {
                    printf("\nNo time slots found.\n");
                    break;
                }


                // Load classes

                Session sessions[MAX_SESSIONS];

                int sessionCount =
                    loadSessions(
                        conn,
                        sessions
                    );


                if (sessionCount == 0)
                {
                    printf("\nNo scheduled classes found.\n");
                    break;
                }


                printf(
                    "\n%d scheduled classes loaded.\n",
                    sessionCount
                );


                // Display database data

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


                for (int i = 0;
                     i < sessionCount;
                     i++)
                {
                    color[i] = -1;
                }


                // We currently have
                // MAX_SLOTS possible slots

                int totalSlots = MAX_SLOTS;


                if (graphColoring(
                        &graph,
                        color,
                        totalSlots,
                        0))
                {
                    printf(
                        "\nTimetable generated successfully!\n"
                    );


                    displayTimetable(
                        sessions,
                        slots,
                        color,
                        sessionCount
                    );
                }
                else
                {
                    printf(
                        "\nNo valid timetable could be generated.\n"
                    );
                }

                break;
            }
            
            case 3:
            {
                int session_id;

                printf("\n========================================\n");
                printf("           CANCEL CLASS\n");
                printf("========================================\n");

                printf("\nEnter Session ID to cancel: ");
                scanf("%d", &session_id);

                char query[500];

                snprintf(
                    query,
                    sizeof(query),
                    "UPDATE class_sessions "
                    "SET status = 'CANCELLED' "
                    "WHERE session_id = %d "
                    "AND faculty_id = %d "
                    "AND status = 'SCHEDULED'",
                    session_id,
                    current_user_id
                );

                if (mysql_query(conn, query) != 0)
                {
                    printf("\nFailed to cancel class: %s\n",
                           mysql_error(conn));
                }
                else if (mysql_affected_rows(conn) == 0)
                {
                    printf("\nNo scheduled class found for this teacher.\n");
                }
                else
                {
                    printf("\nClass cancelled successfully!\n");

                    /*
                     * TODO:
                     * Create notifications for other teachers
                     * assigned to the same panel.
                     */
                }

                break;
            }


            // EXIT

            case 0:

                printf("\nGoodbye!\n");

                mysql_close(conn);

                return 0;


            default:

                printf(
                    "\nInvalid choice. Please try again.\n"
                );
        }
    }


    // This technically won't be reached because
    // case 0 closes the connection.

    mysql_close(conn);

    return 0;
}