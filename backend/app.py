"""
FastAPI backend for UniTimeTableConflictOptimizer.

Architecture:
    React/Vite -> FastAPI -> MySQL
                         ^
                         |
                    scheduler.c

The existing scheduler.c and this API share the same MySQL database.
The C program is currently interactive, so /api/timetable/generate
reimplements only its conflict-graph + backtracking coloring step as a
temporary API-side implementation. All data still comes from MySQL.

Run:
    python3 -m venv .venv
    source .venv/bin/activate
    pip install -r requirements.txt
    uvicorn app:app --reload --port 8000
"""

import os
from typing import Optional

import pymysql
from dotenv import load_dotenv
from fastapi import FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

load_dotenv()

app = FastAPI(title="UniTimeTableConflictOptimizer API", version="1.0.0")

# React runs on localhost:5173 during Vite development.
app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://localhost:5173",
        "http://127.0.0.1:5173",
    ],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

DB_HOST = "localhost"
DB_PORT = 3306
DB_USER = "root"
DB_PASSWORD = "MmM$1321"
DB_NAME = "TimeTableMiniProject"


def get_conn():
    try:
        return pymysql.connect(
            host=DB_HOST,
            port=DB_PORT,
            user=DB_USER,
            password=DB_PASSWORD,
            database=DB_NAME,
            cursorclass=pymysql.cursors.DictCursor,
            autocommit=False,
        )
    except pymysql.MySQLError:
        raise HTTPException(status_code=500, detail="Could not connect to the database.")


def db_error():
    raise HTTPException(status_code=500, detail="A database operation failed.")


def fetch_all(sql: str, params=()):
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            cur.execute(sql, params)
            return cur.fetchall()
    except pymysql.MySQLError:
        db_error()
    finally:
        conn.close()


class FacultyBody(BaseModel):
    faculty_id: int


@app.get("/")
def root():
    return {"name": "UniTimeTableConflictOptimizer API", "status": "running"}


@app.get("/health")
def health():
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            cur.execute("SELECT 1 AS ok")
            cur.fetchone()
        return {"status": "ok", "database": "connected"}
    except pymysql.MySQLError:
        raise HTTPException(status_code=503, detail="Database is unavailable.")
    finally:
        conn.close()


@app.get("/api/faculty")
def get_faculty():
    return fetch_all(
        """
        SELECT
            f.faculty_id,
            f.name,
            COALESCE(
                GROUP_CONCAT(
                    DISTINCT c.course_name
                    ORDER BY c.course_name
                    SEPARATOR ', '
                ),
                'No subject assigned'
            ) AS subjects
        FROM faculty f
        LEFT JOIN faculty_assignments fa
            ON fa.faculty_id = f.faculty_id
        LEFT JOIN courses c
            ON c.course_id = fa.course_id
        WHERE f.faculty_id IN (1,2,3)
        GROUP BY f.faculty_id, f.name
        ORDER BY f.faculty_id
        """
    )


SESSION_SELECT = """
SELECT
    cs.session_id,
    cs.course_id,
    cs.faculty_id,
    cs.panel_id,
    cs.room_id,
    cs.slot_id,
    c.course_name,
    f.name AS faculty_name,
    p.panel_name,
    r.room_number,
    ts.day_of_week,
    TIME_FORMAT(ts.start_time, '%%H:%%i') AS start_time,
    TIME_FORMAT(ts.end_time, '%%H:%%i') AS end_time,
    cs.session_type,
    cs.status
FROM class_sessions cs
JOIN courses c ON cs.course_id = c.course_id
JOIN faculty f ON cs.faculty_id = f.faculty_id
JOIN panels p ON cs.panel_id = p.panel_id
JOIN rooms r ON cs.room_id = r.room_id
JOIN time_slots ts ON cs.slot_id = ts.slot_id
"""


@app.get("/api/timetable/{faculty_id}")
def get_timetable_for_faculty(faculty_id: int):
    return fetch_all(
        SESSION_SELECT +
        " WHERE cs.faculty_id = %s ORDER BY cs.session_id",
        (faculty_id,),
    )


@app.get("/api/timetable")
def get_all_scheduled_sessions():
    return fetch_all(
        SESSION_SELECT +
        " WHERE cs.status = 'SCHEDULED' ORDER BY cs.session_id"
    )


@app.get("/api/notifications/{faculty_id}")
def get_notifications(faculty_id: int):
    return fetch_all(
        """
        SELECT notification_id, session_id, message, response_status
        FROM notifications
        WHERE faculty_id = %s
          AND notification_type = 'AVAILABLE_SLOT'
          AND response_status = 'PENDING'
        ORDER BY notification_id
        """,
        (faculty_id,),
    )


@app.post("/api/notifications/{notification_id}/accept")
def accept_notification(notification_id: int, body: FacultyBody):
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            # Lock the notification row while we perform the checks.
            cur.execute(
                """
                SELECT session_id, makeup_request_id,
                       available_room_id, available_slot_id
                FROM notifications
                WHERE notification_id = %s
                  AND faculty_id = %s
                  AND notification_type = 'AVAILABLE_SLOT'
                  AND response_status = 'PENDING'
                FOR UPDATE
                """,
                (notification_id, body.faculty_id),
            )
            n = cur.fetchone()
            if not n:
                raise HTTPException(
                    status_code=409,
                    detail="Notification is no longer available.",
                )

            cur.execute(
                "SELECT status FROM class_sessions WHERE session_id = %s FOR UPDATE",
                (n["session_id"],),
            )
            released = cur.fetchone()
            if not released or released["status"] != "CANCELLED":
                raise HTTPException(
                    status_code=409,
                    detail="This slot is no longer available.",
                )

            cur.execute(
                """
                SELECT COUNT(*) AS count
                FROM class_sessions
                WHERE room_id = %s
                  AND slot_id = %s
                  AND status = 'SCHEDULED'
                """,
                (n["available_room_id"], n["available_slot_id"]),
            )
            if cur.fetchone()["count"] > 0:
                raise HTTPException(
                    status_code=409,
                    detail="The room is no longer available.",
                )

            cur.execute(
                """
                SELECT COUNT(*) AS count
                FROM class_sessions
                WHERE faculty_id = %s
                  AND slot_id = %s
                  AND status = 'SCHEDULED'
                """,
                (body.faculty_id, n["available_slot_id"]),
            )
            if cur.fetchone()["count"] > 0:
                raise HTTPException(
                    status_code=409,
                    detail="You already have a class during this slot.",
                )

            cur.execute(
                """
                INSERT INTO class_sessions
                    (course_id, faculty_id, panel_id, room_id, slot_id,
                     status, session_type)
                SELECT
                    cs.course_id,
                    %s,
                    cs.panel_id,
                    %s,
                    %s,
                    'SCHEDULED',
                    'MAKEUP'
                FROM makeup_requests mr
                JOIN class_sessions cs ON mr.session_id = cs.session_id
                WHERE mr.request_id = %s
                  AND mr.faculty_id = %s
                  AND mr.status = 'PENDING'
                """,
                (
                    body.faculty_id,
                    n["available_room_id"],
                    n["available_slot_id"],
                    n["makeup_request_id"],
                    body.faculty_id,
                ),
            )
            if cur.rowcount == 0:
                raise HTTPException(
                    status_code=409,
                    detail="Makeup request is no longer pending.",
                )

            cur.execute(
                """
                UPDATE makeup_requests
                SET status = 'FULFILLED'
                WHERE request_id = %s
                """,
                (n["makeup_request_id"],),
            )

            cur.execute(
                """
                UPDATE notifications
                SET response_status = 'ACCEPTED', is_read = 1
                WHERE notification_id = %s
                """,
                (notification_id,),
            )

            cur.execute(
                """
                UPDATE notifications
                SET response_status = 'EXPIRED', is_read = 1
                WHERE session_id = %s
                  AND makeup_request_id = %s
                  AND notification_id <> %s
                  AND response_status = 'PENDING'
                """,
                (n["session_id"], n["makeup_request_id"], notification_id),
            )

        conn.commit()
        return {"message": "Makeup class accepted."}

    except HTTPException:
        conn.rollback()
        raise
    except pymysql.MySQLError:
        conn.rollback()
        db_error()
    finally:
        conn.close()


@app.post("/api/notifications/{notification_id}/decline")
def decline_notification(notification_id: int, body: FacultyBody):
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            cur.execute(
                """
                UPDATE notifications
                SET response_status = 'DECLINED', is_read = 1
                WHERE notification_id = %s
                  AND faculty_id = %s
                  AND notification_type = 'AVAILABLE_SLOT'
                  AND response_status = 'PENDING'
                """,
                (notification_id, body.faculty_id),
            )
            if cur.rowcount == 0:
                raise HTTPException(
                    status_code=409,
                    detail="Notification is no longer available.",
                )
        conn.commit()
        return {"message": "Notification declined."}
    except HTTPException:
        conn.rollback()
        raise
    except pymysql.MySQLError:
        conn.rollback()
        db_error()
    finally:
        conn.close()


@app.post("/api/classes/{session_id}/cancel")
def cancel_class(session_id: int, body: FacultyBody):
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            cur.execute(
                """
                UPDATE class_sessions
                SET status = 'CANCELLED'
                WHERE session_id = %s
                  AND faculty_id = %s
                  AND status = 'SCHEDULED'
                """,
                (session_id, body.faculty_id),
            )
            if cur.rowcount == 0:
                raise HTTPException(
                    status_code=409,
                    detail="Class could not be cancelled.",
                )

            # Mirrors createAvailableSlotNotifications() in scheduler.c.
            cur.execute(
                """
                DELETE FROM notifications
                WHERE session_id = %s
                  AND notification_type = 'AVAILABLE_SLOT'
                """,
                (session_id,),
            )

            cur.execute(
                """
                INSERT INTO notifications
                    (faculty_id, session_id, makeup_request_id,
                     available_room_id, available_slot_id,
                     message, notification_type, response_status)
                SELECT
                    mr.faculty_id,
                    released.session_id,
                    mr.request_id,
                    released.room_id,
                    released.slot_id,
                    CONCAT(
                        'Makeup slot available for ', c.course_name,
                        ': ', ts.day_of_week, ' ',
                        ts.start_time, '-', ts.end_time,
                        ', Room ', r.room_number
                    ),
                    'AVAILABLE_SLOT',
                    'PENDING'
                FROM makeup_requests mr
                JOIN class_sessions original
                    ON mr.session_id = original.session_id
                JOIN courses c
                    ON original.course_id = c.course_id
                JOIN class_sessions released
                    ON released.session_id = %s
                JOIN panels p
                    ON released.panel_id = p.panel_id
                JOIN time_slots ts
                    ON released.slot_id = ts.slot_id
                JOIN rooms r
                    ON released.room_id = r.room_id
                JOIN faculty_assignments fa
                    ON fa.faculty_id = mr.faculty_id
                   AND fa.panel_id = released.panel_id
                WHERE mr.status = 'PENDING'
                  AND released.status = 'CANCELLED'
                  AND mr.faculty_id <> released.faculty_id
                  AND NOT EXISTS (
                      SELECT 1
                      FROM notifications n
                      WHERE n.faculty_id = mr.faculty_id
                        AND n.session_id = released.session_id
                        AND n.makeup_request_id = mr.request_id
                        AND n.notification_type = 'AVAILABLE_SLOT'
                        AND n.response_status = 'PENDING'
                  )
                """,
                (session_id,),
            )

        conn.commit()
        return {
            "message": "Class cancelled. Other eligible faculty have been notified."
        }
    except HTTPException:
        conn.rollback()
        raise
    except pymysql.MySQLError:
        conn.rollback()
        db_error()
    finally:
        conn.close()


@app.get("/api/makeup-requests/{faculty_id}")
def get_makeup_requests(faculty_id: int):
    return fetch_all(
        """
        SELECT
            mr.request_id,
            c.course_name,
            mr.priority,
            mr.status
        FROM makeup_requests mr
        JOIN class_sessions cs ON mr.session_id = cs.session_id
        JOIN courses c ON cs.course_id = c.course_id
        WHERE mr.faculty_id = %s
        ORDER BY mr.priority DESC
        """,
        (faculty_id,),
    )


@app.get("/api/time-slots")
def get_time_slots():
    return fetch_all(
        """
        SELECT
            slot_id,
            day_of_week,
            TIME_FORMAT(start_time, '%%H:%%i') AS start_time,
            TIME_FORMAT(end_time, '%%H:%%i') AS end_time
        FROM time_slots
        ORDER BY slot_id
        """
    )


@app.get("/api/rooms")
def get_rooms():
    return fetch_all(
        "SELECT room_id, room_number FROM rooms ORDER BY room_id"
    )


@app.get("/api/rooms/available")
def get_available_rooms(slotId: int = Query(...)):
    return fetch_all(
        """
        SELECT
            r.room_id,
            r.room_number,
            CASE
                WHEN EXISTS (
                    SELECT 1
                    FROM class_sessions cs
                    WHERE cs.room_id = r.room_id
                      AND cs.slot_id = %s
                      AND cs.status = 'SCHEDULED'
                )
                THEN FALSE
                ELSE TRUE
            END AS available
        FROM rooms r
        ORDER BY r.room_id
        """,
        (slotId,),
    )


# ---------------- Timetable generation ----------------
# This mirrors scheduler.c:
#   conflict = same panel OR same faculty
#   MAKEUP keeps its existing slot
#   regular sessions are assigned using backtracking graph coloring.


def build_conflict_graph(sessions):
    n = len(sessions)
    graph = [[] for _ in range(n)]
    edges = []

    for i in range(n):
        for j in range(i + 1, n):
            if (
                sessions[i]["panel_id"] == sessions[j]["panel_id"]
                or sessions[i]["faculty_id"] == sessions[j]["faculty_id"]
            ):
                graph[i].append(j)
                graph[j].append(i)
                edges.append(
                    [sessions[i]["course_name"], sessions[j]["course_name"]]
                )
    return graph, edges


def color_sessions(sessions, slot_ids):
    n = len(sessions)
    colors = [-1] * n
    slot_index = {slot_id: i for i, slot_id in enumerate(slot_ids)}

    for i, s in enumerate(sessions):
        if s["session_type"] == "MAKEUP" and s["slot_id"] in slot_index:
            colors[i] = slot_index[s["slot_id"]]

    # Same backtracking strategy as the C implementation:
    # choose the first uncolored vertex and try each slot.
    def safe(v, color):
        return all(colors[u] != color for u in range(n) if False)

    def is_safe(v, color, graph):
        return all(colors[u] != color for u in graph[v])

    def backtrack(v):
        if v == n:
            return True

        # Skip already-fixed MAKEUP sessions.
        if colors[v] != -1:
            return backtrack(v + 1)

        for color in range(len(slot_ids)):
            if is_safe(v, color, graph):
                colors[v] = color
                if backtrack(v + 1):
                    return True
                colors[v] = -1

        return False

    graph, _ = build_conflict_graph(sessions)

    if not backtrack(0):
        return None, graph

    return colors, graph


@app.post("/api/timetable/generate")
def generate_timetable():
    slots = fetch_all(
        "SELECT slot_id, day_of_week, "
        "TIME_FORMAT(start_time, '%%H:%%i') AS start_time,"
        "TIME_FORMAT(end_time, '%%H:%%i') AS end_time "
        "FROM time_slots ORDER BY slot_id"
    )

    sessions = fetch_all(
        SESSION_SELECT +
        " WHERE cs.status = 'SCHEDULED' ORDER BY cs.session_id"
    )

    if not slots:
        raise HTTPException(status_code=409, detail="No time slots found.")
    if not sessions:
        raise HTTPException(status_code=409, detail="No scheduled classes found.")

    slot_ids = [s["slot_id"] for s in slots]
    colors, graph = color_sessions(sessions, slot_ids)

    if colors is None:
        raise HTTPException(
            status_code=409,
            detail="No valid timetable could be generated.",
        )

    # Persist the generated slot assignments for REGULAR sessions.
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            for i, s in enumerate(sessions):
                if s["session_type"] == "MAKEUP":
                    continue

                new_slot_id = slot_ids[colors[i]]
                cur.execute(
                    """
                    UPDATE class_sessions
                    SET slot_id = %s
                    WHERE session_id = %s
                      AND status = 'SCHEDULED'
                    """,
                    (new_slot_id, s["session_id"]),
                )

        conn.commit()
    except pymysql.MySQLError:
        conn.rollback()
        db_error()
    finally:
        conn.close()

    # Re-read after updates so the response contains final day/time values.
    final_sessions = fetch_all(
        SESSION_SELECT +
        " WHERE cs.status = 'SCHEDULED' ORDER BY cs.session_id"
    )

    _, conflict_edges = build_conflict_graph(sessions)

    return {
        "sessions": final_sessions,
        "conflictGraph": conflict_edges,
    }
