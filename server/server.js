import express from 'express';
import cors from 'cors';
import dotenv from 'dotenv';
import mysql from 'mysql2/promise';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

dotenv.config();
const app = express();
const PORT = Number(process.env.PORT || 3001);
const DEMO_MODE = String(process.env.DEMO_MODE || 'true').toLowerCase() === 'true';
const __dirname = path.dirname(fileURLToPath(import.meta.url));
const execFileAsync = promisify(execFile);

app.use(cors());
app.use(express.json());

const pool = mysql.createPool({
  host: process.env.DB_HOST,
  user: process.env.DB_USER,
  password: process.env.DB_PASSWORD,
  database: process.env.DB_NAME,
  port: Number(process.env.DB_PORT || 3306),
  waitForConnections: true,
  connectionLimit: 5,
  namedPlaceholders: true
});

const demoSlots = [
  { slot_id: 1, day_of_week: 'Monday', start_time: '09:00:00', end_time: '10:00:00' },
  { slot_id: 2, day_of_week: 'Monday', start_time: '10:00:00', end_time: '11:00:00' },
  { slot_id: 3, day_of_week: 'Monday', start_time: '11:00:00', end_time: '12:00:00' },
  { slot_id: 4, day_of_week: 'Tuesday', start_time: '09:00:00', end_time: '10:00:00' },
  { slot_id: 5, day_of_week: 'Tuesday', start_time: '10:00:00', end_time: '11:00:00' },
  { slot_id: 6, day_of_week: 'Tuesday', start_time: '11:00:00', end_time: '12:00:00' }
];

function sendData(res, data) { res.json({ ok: true, data }); }
function dbError(res, err) { console.error(err); res.status(500).json({ ok: false, error: 'Database operation failed.' }); }

app.get('/api/health', async (_req, res) => {
  if (DEMO_MODE) return sendData(res, { mode: 'demo' });
  try { await pool.query('SELECT 1'); sendData(res, { mode: 'mysql' }); }
  catch (err) { dbError(res, err); }
});

app.get('/api/faculty', async (_req, res) => {
  if (DEMO_MODE) return sendData(res, [
    { faculty_id: 1, name: 'Prathm' },
    { faculty_id: 2, name: 'Vrukshita' },
    { faculty_id: 3, name: 'Shreya' }
  ]);
  try { const [rows] = await pool.query('SELECT faculty_id, name FROM faculty ORDER BY faculty_id'); sendData(res, rows); }
  catch (err) { dbError(res, err); }
});

app.get('/api/time-slots', async (_req, res) => {
  if (DEMO_MODE) return sendData(res, demoSlots);
  try { const [rows] = await pool.query('SELECT slot_id, day_of_week, start_time, end_time FROM time_slots ORDER BY slot_id'); sendData(res, rows); }
  catch (err) { dbError(res, err); }
});

app.get('/api/timetable/:facultyId', async (req, res) => {
  const facultyId = Number(req.params.facultyId);
  if (DEMO_MODE) return sendData(res, []);
  try {
    const [rows] = await pool.query(`
      SELECT cs.session_id, cs.course_id, cs.faculty_id, cs.panel_id, cs.room_id, cs.slot_id,
             cs.status, cs.session_type,
             c.course_name,
             f.name AS faculty_name,
             p.panel_name,
             r.room_number,
             ts.day_of_week, ts.start_time, ts.end_time
      FROM class_sessions cs
      JOIN courses c ON cs.course_id = c.course_id
      JOIN faculty f ON cs.faculty_id = f.faculty_id
      JOIN panels p ON cs.panel_id = p.panel_id
      JOIN rooms r ON cs.room_id = r.room_id
      JOIN time_slots ts ON cs.slot_id = ts.slot_id
      WHERE cs.status = 'SCHEDULED'
        AND cs.faculty_id = :facultyId
      ORDER BY ts.slot_id, cs.session_id`, { facultyId });
    sendData(res, rows);
  } catch (err) { dbError(res, err); }
});

app.get('/api/notifications/:facultyId', async (req, res) => {
  const facultyId = Number(req.params.facultyId);
  if (DEMO_MODE) return sendData(res, []);
  try {
    const [rows] = await pool.query(`
      SELECT notification_id, faculty_id, session_id, message, notification_type,
             response_status, is_read, created_at
      FROM notifications
      WHERE faculty_id = :facultyId
      ORDER BY notification_id DESC`, { facultyId });
    sendData(res, rows);
  } catch (err) { dbError(res, err); }
});

app.get('/api/makeup-requests/:facultyId', async (req, res) => {
  const facultyId = Number(req.params.facultyId);
  if (DEMO_MODE) return sendData(res, []);
  try {
    const [rows] = await pool.query(`
      SELECT mr.request_id, mr.session_id, mr.faculty_id, mr.priority, mr.status,
             c.course_name
      FROM makeup_requests mr
      JOIN class_sessions cs ON mr.session_id = cs.session_id
      JOIN courses c ON cs.course_id = c.course_id
      WHERE mr.faculty_id = :facultyId
      ORDER BY mr.priority DESC, mr.request_id`, { facultyId });
    sendData(res, rows);
  } catch (err) { dbError(res, err); }
});

app.post('/api/classes/:sessionId/cancel', async (req, res) => {
  const sessionId = Number(req.params.sessionId);
  const facultyId = Number(req.body?.faculty_id);
  if (DEMO_MODE) return sendData(res, { status: 'CANCELLED' });
  try {
    const [result] = await pool.query(`
      UPDATE class_sessions
      SET status = 'CANCELLED'
      WHERE session_id = :sessionId
        AND faculty_id = :facultyId
        AND status = 'SCHEDULED'`, { sessionId, facultyId });
    if (!result.affectedRows) return res.status(409).json({ ok: false, error: 'Class not found, not assigned to you, or already cancelled.' });

    // Reuse the same notification model as the C program.
    await pool.query(`
      INSERT INTO notifications (faculty_id, session_id, message, notification_type, response_status)
      SELECT fa.faculty_id, cs.session_id,
             CONCAT('A class slot has become available for ', c.course_name, ': ', p.panel_name, ', ', ts.day_of_week, ' ', ts.start_time, '-', ts.end_time, ', Room ', r.room_number),
             'AVAILABLE_SLOT', 'PENDING'
      FROM class_sessions cs
      JOIN courses c ON cs.course_id = c.course_id
      JOIN panels p ON cs.panel_id = p.panel_id
      JOIN time_slots ts ON cs.slot_id = ts.slot_id
      JOIN rooms r ON cs.room_id = r.room_id
      JOIN faculty_assignments fa ON fa.panel_id = cs.panel_id
      WHERE cs.session_id = :sessionId
        AND cs.status = 'CANCELLED'
        AND fa.faculty_id <> cs.faculty_id
        AND NOT EXISTS (
          SELECT 1 FROM notifications n
          WHERE n.faculty_id = fa.faculty_id
            AND n.session_id = cs.session_id
            AND n.notification_type = 'AVAILABLE_SLOT'
            AND n.response_status = 'PENDING'
        )`, { sessionId });
    sendData(res, { status: 'CANCELLED' });
  } catch (err) { dbError(res, err); }
});

app.post('/api/notifications/:notificationId/decline', async (req, res) => {
  const notificationId = Number(req.params.notificationId);
  const facultyId = Number(req.body?.faculty_id);
  if (DEMO_MODE) return sendData(res, { response_status: 'DECLINED' });
  try {
    const [result] = await pool.query(`
      UPDATE notifications
      SET response_status = 'DECLINED', is_read = 1
      WHERE notification_id = :notificationId
        AND faculty_id = :facultyId
        AND response_status = 'PENDING'`, { notificationId, facultyId });
    if (!result.affectedRows) return res.status(409).json({ ok: false, error: 'Notification is no longer pending.' });
    sendData(res, { response_status: 'DECLINED' });
  } catch (err) { dbError(res, err); }
});

app.post('/api/notifications/:notificationId/accept', async (req, res) => {
  const notificationId = Number(req.params.notificationId);
  const facultyId = Number(req.body?.faculty_id);
  const makeupRequestId = Number(req.body?.makeup_request_id);
  if (DEMO_MODE) return sendData(res, { response_status: 'ACCEPTED', session_type: 'MAKEUP' });
  if (!makeupRequestId) return res.status(400).json({ ok: false, error: 'makeup_request_id is required.' });

  const conn = await pool.getConnection();
  try {
    await conn.beginTransaction();
    const [[notif]] = await conn.query(`
      SELECT session_id FROM notifications
      WHERE notification_id = :notificationId
        AND faculty_id = :facultyId
        AND notification_type = 'AVAILABLE_SLOT'
        AND response_status = 'PENDING'`, { notificationId, facultyId });
    if (!notif) { await conn.rollback(); return res.status(409).json({ ok: false, error: 'Notification is no longer available.' }); }

    const [[released]] = await conn.query(`
      SELECT room_id, slot_id, panel_id, status
      FROM class_sessions WHERE session_id = :sessionId`, { sessionId: notif.session_id });
    if (!released || released.status !== 'CANCELLED') { await conn.rollback(); return res.status(409).json({ ok: false, error: 'Released slot is no longer available.' }); }

    const [[request]] = await conn.query(`
      SELECT mr.request_id, cs.course_id, cs.panel_id
      FROM makeup_requests mr
      JOIN class_sessions cs ON mr.session_id = cs.session_id
      WHERE mr.request_id = :makeupRequestId
        AND mr.faculty_id = :facultyId
        AND mr.status = 'PENDING'`, { makeupRequestId, facultyId });
    if (!request) { await conn.rollback(); return res.status(409).json({ ok: false, error: 'Makeup request is no longer pending.' }); }

    const [[roomConflict]] = await conn.query(`SELECT COUNT(*) AS count FROM class_sessions WHERE room_id = :roomId AND slot_id = :slotId AND status = 'SCHEDULED'`, { roomId: released.room_id, slotId: released.slot_id });
    const [[facultyConflict]] = await conn.query(`SELECT COUNT(*) AS count FROM class_sessions WHERE faculty_id = :facultyId AND slot_id = :slotId AND status = 'SCHEDULED'`, { facultyId, slotId: released.slot_id });
    const [[panelConflict]] = await conn.query(`SELECT COUNT(*) AS count FROM class_sessions WHERE panel_id = :panelId AND slot_id = :slotId AND status = 'SCHEDULED'`, { panelId: request.panel_id, slotId: released.slot_id });
    if (roomConflict.count || facultyConflict.count || panelConflict.count) { await conn.rollback(); return res.status(409).json({ ok: false, error: 'Room, faculty, or panel conflict detected.' }); }

    await conn.query(`
      INSERT INTO class_sessions (course_id, faculty_id, panel_id, room_id, slot_id, status, session_type)
      VALUES (:courseId, :facultyId, :panelId, :roomId, :slotId, 'SCHEDULED', 'MAKEUP')`, {
        courseId: request.course_id, facultyId, panelId: request.panel_id,
        roomId: released.room_id, slotId: released.slot_id
      });

    await conn.query(`UPDATE makeup_requests SET status = 'FULFILLED' WHERE request_id = :makeupRequestId`, { makeupRequestId });
    await conn.query(`UPDATE notifications SET response_status = 'ACCEPTED', is_read = 1 WHERE notification_id = :notificationId`, { notificationId });
    await conn.query(`UPDATE notifications SET response_status = 'EXPIRED', is_read = 1 WHERE session_id = :sessionId AND notification_id <> :notificationId AND response_status = 'PENDING'`, { sessionId: notif.session_id, notificationId });
    await conn.commit();
    sendData(res, { response_status: 'ACCEPTED', session_type: 'MAKEUP' });
  } catch (err) {
    await conn.rollback();
    dbError(res, err);
  } finally { conn.release(); }
});

app.get('/api/rooms/available', async (req, res) => {
  const slotId = Number(req.query.slotId);
  if (DEMO_MODE) return sendData(res, [
    { room_id: 1, room_number: 'A-101', available: true },
    { room_id: 2, room_number: 'A-102', available: true },
    { room_id: 3, room_number: 'A-201', available: true },
    { room_id: 4, room_number: 'LAB-1', available: true }
  ]);
  try {
    const [rows] = await pool.query(`
      SELECT r.room_id, r.room_number,
             CASE WHEN EXISTS (
               SELECT 1 FROM class_sessions cs
               WHERE cs.room_id = r.room_id
                 AND cs.slot_id = :slotId
                 AND cs.status = 'SCHEDULED'
             ) THEN 0 ELSE 1 END AS available
      FROM rooms r
      ORDER BY r.room_id`, { slotId });
    sendData(res, rows.map(r => ({ ...r, available: Boolean(r.available) })));
  } catch (err) { dbError(res, err); }
});

app.post('/api/timetable/generate', async (_req, res) => {
  // The present C scheduler is interactive (password + faculty menu), so blindly spawning it
  // from a web request would be unreliable. We intentionally leave the bridge configurable.
  // Set SCHEDULER_EXECUTABLE to a non-interactive wrapper/executable that prints JSON when ready.
  const executable = process.env.SCHEDULER_EXECUTABLE;
  if (!executable) return res.status(501).json({ ok: false, error: 'C scheduler bridge not configured. Set SCHEDULER_EXECUTABLE to a non-interactive scheduler wrapper.' });
  try {
    const { stdout } = await execFileAsync(executable, [], { cwd: path.resolve(__dirname, '..'), timeout: 20000 });
    const marker = stdout.lastIndexOf('{');
    const parsed = marker >= 0 ? JSON.parse(stdout.slice(marker)) : [];
    sendData(res, parsed);
  } catch (err) { dbError(res, err); }
});

app.listen(PORT, () => {
  console.log(`UniTimeTable API running on http://localhost:${PORT}`);
  console.log(`Mode: ${DEMO_MODE ? 'DEMO' : 'MYSQL'}`);
});
