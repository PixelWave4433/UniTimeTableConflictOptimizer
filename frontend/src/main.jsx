import React, { useEffect, useMemo, useState } from 'react';
import { createRoot } from 'react-dom/client';
import './styles.css';

const FACULTY = [
  { faculty_id: 1, name: 'Prathm' },
  { faculty_id: 2, name: 'Vrukshita' },
  { faculty_id: 3, name: 'Shreya' }
];

const DEMO_SLOTS = [
  { slot_id: 1, day_of_week: 'Monday', start_time: '09:00:00', end_time: '10:00:00' },
  { slot_id: 2, day_of_week: 'Monday', start_time: '10:00:00', end_time: '11:00:00' },
  { slot_id: 3, day_of_week: 'Monday', start_time: '11:00:00', end_time: '12:00:00' },
  { slot_id: 4, day_of_week: 'Tuesday', start_time: '09:00:00', end_time: '10:00:00' },
  { slot_id: 5, day_of_week: 'Tuesday', start_time: '10:00:00', end_time: '11:00:00' },
  { slot_id: 6, day_of_week: 'Tuesday', start_time: '11:00:00', end_time: '12:00:00' }
];

const DEMO_TIMETABLE = [
  { session_id: 1, course_id: 1, course_name: 'Drawing', faculty_id: 1, faculty_name: 'Prathm', panel_name: 'CSF-A', room_number: 'A-101', slot_id: 1, day_of_week: 'Monday', start_time: '09:00:00', end_time: '10:00:00', session_type: 'REGULAR', status: 'SCHEDULED' },
  { session_id: 3, course_id: 3, course_name: 'LADC', faculty_id: 3, faculty_name: 'Shreya', panel_name: 'CSF-A', room_number: 'A-201', slot_id: 3, day_of_week: 'Monday', start_time: '11:00:00', end_time: '12:00:00', session_type: 'REGULAR', status: 'SCHEDULED' },
  { session_id: 4, course_id: 1, course_name: 'Drawing', faculty_id: 1, faculty_name: 'Prathm', panel_name: 'CSF-A', room_number: 'A-102', slot_id: 2, day_of_week: 'Monday', start_time: '10:00:00', end_time: '11:00:00', session_type: 'MAKEUP', status: 'SCHEDULED' }
];

const DEMO_NOTIFICATIONS = [
  {
    notification_id: 21,
    faculty_id: 1,
    session_id: 54,
    message: 'A class slot has become available for Dance: CSF-A, Monday 10:00:00-11:00:00, Room A-102',
    notification_type: 'AVAILABLE_SLOT',
    response_status: 'PENDING',
    is_read: 0,
    created_at: '2026-09-27 08:30:00'
  }
];

function App() {
  const [faculty, setFaculty] = useState(null);
  const [facultyList, setFacultyList] = useState([]);
  const [page, setPage] = useState('dashboard');
  const [timetable, setTimetable] = useState([]);
  const [notifications, setNotifications] = useState([]);
  const [slots, setSlots] = useState(DEMO_SLOTS);
  const [rooms, setRooms] = useState([]);
  const [makeups, setMakeups] = useState([]);
  const [loading, setLoading] = useState(false);
  const [toast, setToast] = useState(null);
  const [searchSlot, setSearchSlot] = useState(1);
  const [showLogin, setShowLogin] = useState(true);
  const [makeupModal, setMakeupModal] = useState(null);

  const loadAll = async (selectedFaculty = faculty) => {
    setLoading(true);
    try {
      const [tt, notif, sl, makeup] = await Promise.all([
        api(`/api/timetable/${selectedFaculty.faculty_id}`),
        api(`/api/notifications/${selectedFaculty.faculty_id}`),
        api('/api/time-slots'),
        api(`/api/makeup-requests/${selectedFaculty.faculty_id}`)
      ]);

      setTimetable(Array.isArray(tt) ? tt : []);
      setNotifications(Array.isArray(notif) ? notif : []);
      setSlots(Array.isArray(sl) && sl.length ? sl : []);
      setMakeups(Array.isArray(makeup) ? makeup : []);
    }
    /*
    catch (err) {
      // Demo data is intentionally available when the API isn't connected yet.
      setTimetable(DEMO_TIMETABLE.filter(x => x.faculty_id === selectedFaculty.faculty_id || x.panel_name === 'CSF-A'));
      setNotifications(DEMO_NOTIFICATIONS.filter(x => x.faculty_id === selectedFaculty.faculty_id));
      setSlots(DEMO_SLOTS);
      setMakeups(selectedFaculty.faculty_id === 1 ? [{ request_id: 1, session_id: 1, faculty_id: 1, course_name: 'Drawing', priority: 5, status: 'PENDING' }] : []);
    } finally {*/
    // just a test
     catch (err) {
      console.error(err);
      setToast({
        type: 'error',
        text: 'Could not connect to FastAPI.'
      });
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    const loadFaculty = async () => {
      try {
        const data = await api('/api/faculty');
        setFacultyList(Array.isArray(data) ? data : []);
      } catch (err) {
        console.error('Failed to load faculty:', err);
      }
    };

    loadFaculty();
  }, []);

  useEffect(() => {
    if (!showLogin) loadAll(faculty);
  }, [showLogin]);

  const pendingCount = notifications.filter(n => n.response_status === 'PENDING').length;
  const cancelledCount = Math.max(0, timetable.filter(x => x.status === 'CANCELLED').length);
  const regularCount = timetable.filter(x => x.session_type === 'REGULAR').length;
  const makeupCount = timetable.filter(x => x.session_type === 'MAKEUP').length;

  const currentSlots = useMemo(() => slots, [slots]);

  const navigate = (next) => {
    setPage(next);
    setToast(null);
  };

  const handleFaculty = async (f) => {
    setFaculty(f);
    setShowLogin(false);
    setPage('dashboard');
    await loadAll(f);
  };

  const refresh = () => loadAll(faculty);

  const acceptNotification = async (notification) => {
    const candidates = makeups.length ? makeups : [];
    if (!candidates.length) {
      setToast({ type: 'info', text: 'No pending makeup request exists for this faculty member.' });
      return;
    }
    setMakeupModal(notification);
  };

  const completeMakeup = async (requestId) => {
    try {
      await api(`/api/notifications/${makeupModal.notification_id}/accept`, {
        method: 'POST',
        body: JSON.stringify({ faculty_id: faculty.faculty_id, makeup_request_id: requestId })
      });
      setToast({ type: 'success', text: 'Makeup class accepted and scheduled.' });
      setMakeupModal(null);
      await refresh();
    } catch (err) {
      setToast({ type: 'success', text: 'Demo mode: makeup class accepted.' });
      setMakeupModal(null);
      setNotifications(prev => prev.map(n => n.notification_id === makeupModal.notification_id ? { ...n, response_status: 'ACCEPTED', is_read: 1 } : n));
    }
  };

  const declineNotification = async (notificationId) => {
    try {
      await api(`/api/notifications/${notificationId}/decline`, {
        method: 'POST',
        body: JSON.stringify({ faculty_id: faculty.faculty_id })
      });
      setToast({ type: 'success', text: 'Notification declined.' });
      await refresh();
    } catch (err) {
      setToast({ type: 'success', text: 'Demo mode: notification declined.' });
      setNotifications(prev => prev.map(n => n.notification_id === notificationId ? { ...n, response_status: 'DECLINED', is_read: 1 } : n));
    }
  };

  const cancelSession = async (sessionId) => {
    if (!window.confirm('Cancel this class session?')) return;
    try {
      await api(`/api/classes/${sessionId}/cancel`, {
        method: 'POST',
        body: JSON.stringify({ faculty_id: faculty.faculty_id })
      });
      setToast({ type: 'success', text: 'Class cancelled and relevant faculty were notified.' });
      await refresh();
    } catch (err) {
      setTimetable(prev => prev.map(x => x.session_id === sessionId ? { ...x, status: 'CANCELLED' } : x));
      setToast({ type: 'success', text: 'Demo mode: class cancelled.' });
    }
  };

  const generate = async () => {
    setLoading(true);
    try {
      const result = await api('/api/timetable/generate', { method: 'POST' });
      setTimetable(Array.isArray(result.sessions) ? result.sessions : []);
      setToast({ type: 'success', text: 'Timetable generated.' });
    } catch (err) {
      setTimetable(DEMO_TIMETABLE);
      setToast({ type: 'success', text: 'Demo timetable loaded. Connect the API to invoke the C scheduler.' });
    } finally {
      setLoading(false);
    }
  };

  const searchRooms = async () => {
    try {
      const result = await api(`/api/rooms/available?slotId=${searchSlot}`);
      setRooms(Array.isArray(result) ? result : []);
    } catch (err) {
      const occupied = new Set(timetable.filter(x => x.slot_id === Number(searchSlot) && x.status === 'SCHEDULED').map(x => x.room_number));
      setRooms([
        { room_id: 1, room_number: 'A-101', available: !occupied.has('A-101') },
        { room_id: 2, room_number: 'A-102', available: !occupied.has('A-102') },
        { room_id: 3, room_number: 'A-201', available: !occupied.has('A-201') },
        { room_id: 4, room_number: 'LAB-1', available: !occupied.has('LAB-1') }
      ]);
    }
  };

  const myClasses = timetable.filter(x => x.faculty_id === faculty.faculty_id);

  if (showLogin) {
    return (
        <Login
            facultyList={facultyList}
            onSelect={handleFaculty}
        />
    );
  }

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="brand">
          <div className="brand-mark">UT</div>
          <div>
            <div className="brand-title">UniTimeTable</div>
            <div className="brand-sub">Conflict Optimizer</div>
          </div>
        </div>

        <nav className="nav-group">
          <NavItem active={page === 'dashboard'} label="Overview" icon="⌂" onClick={() => navigate('dashboard')} />
          <NavItem active={page === 'timetable'} label="My Timetable" icon="▦" onClick={() => navigate('timetable')} />
          <NavItem active={page === 'notifications'} label="Notifications" icon="◉" badge={pendingCount} onClick={() => navigate('notifications')} />
          <NavItem active={page === 'cancel'} label="Cancel Class" icon="×" onClick={() => navigate('cancel')} />
          <NavItem active={page === 'rooms'} label="Room Search" icon="⌖" onClick={() => navigate('rooms')} />
          <NavItem active={page === 'makeups'} label="Makeup Requests" icon="↻" onClick={() => navigate('makeups')} />
        </nav>

        <div className="sidebar-bottom">
          <button className="faculty-switch" onClick={() => setShowLogin(true)}>
            <Avatar name={faculty.name} />

            <div className="faculty-meta">
              <strong>{faculty.name}</strong>
              <span>{faculty.subjects} Teacher</span>
              <span>Faculty ID {faculty.faculty_id}</span>
            </div>

            <span className="chevron">›</span>
          </button>
          <div className="stack-note">DS + DBMS mini project</div>
        </div>
      </aside>

      <main className="main-area">
        <header className="topbar">
          <div>
            <div className="eyebrow">UNIVERSITY SCHEDULING</div>
            <h1>{pageTitle(page)}</h1>
          </div>
          <div className="topbar-actions">
            <button className="profile-chip" onClick={() => setShowLogin(true)}>
              <Avatar name={faculty.name} small />
              <span>{faculty.name} · {faculty.subjects}</span>
            </button>
          </div>
        </header>

        {toast && <Toast toast={toast} onClose={() => setToast(null)} />}

        <section className="content">
          {page === 'dashboard' && (
            <Dashboard
              faculty={faculty}
              pendingCount={pendingCount}
              cancelledCount={cancelledCount}
              regularCount={regularCount}
              makeupCount={makeupCount}
              timetable={timetable}
              notifications={notifications}
              loading={loading}
              onNavigate={navigate}
              onGenerate={generate}
            />
          )}

          {page === 'timetable' && (
              <TimetablePage
                  timetable={timetable}
                  facultyList={facultyList}
                  loading={loading}
                  onGenerate={generate}
              />
          )}

          {page === 'notifications' && (
            <NotificationsPage
              notifications={notifications}
              onAccept={acceptNotification}
              onDecline={declineNotification}
            />
          )}

          {page === 'cancel' && (
            <CancelPage classes={myClasses.filter(x => x.status === 'SCHEDULED')} onCancel={cancelSession} />
          )}

          {page === 'rooms' && (
            <RoomsPage slots={currentSlots} selectedSlot={searchSlot} setSelectedSlot={setSearchSlot} rooms={rooms} onSearch={searchRooms} />
          )}

          {page === 'makeups' && (
            <MakeupsPage makeups={makeups} />
          )}
        </section>
      </main>

      {makeupModal && (
        <Modal title="Choose a makeup class" onClose={() => setMakeupModal(null)}>
          <p className="modal-copy">Select which pending makeup request should use this released slot.</p>
          <div className="request-list">
            {makeups.map(m => (
              <button key={m.request_id} className="request-option" onClick={() => completeMakeup(m.request_id)}>
                <div>
                  <strong>{m.course_name}</strong>
                  <span>Request #{m.request_id}</span>
                </div>
                <span className="priority">Priority {m.priority}</span>
              </button>
            ))}
          </div>
        </Modal>
      )}
    </div>
  );
}

function Login({ facultyList, onSelect }) {
  return (
    <div className="login-screen">
      <div className="login-glow glow-one" />
      <div className="login-glow glow-two" />
      <div className="login-card">
        <div className="brand center-brand">
          <div className="brand-mark large">UT</div>
          <div>
            <div className="brand-title">UniTimeTable</div>
            <div className="brand-sub">Conflict Optimizer</div>
          </div>
        </div>
        <div className="login-kicker">DEMO FACULTY PORTAL</div>
        <h1>Welcome back</h1>
        <p className="muted">Choose a demo faculty account to explore the scheduling workflow.</p>
        <div className="faculty-grid">
          {facultyList.map(f => (
            <button key={f.faculty_id} className="faculty-card" onClick={() => onSelect(f)}>
              <Avatar name={f.name} />
              <div className="faculty-card-copy">
                <strong>{f.name}</strong>
                <span>{f.subjects} Teacher</span>
              </div>
              <span className="arrow">→</span>
            </button>
          ))}
        </div>
        <div className="login-footer">Demo data only • No real teacher information</div>
      </div>
    </div>
  );
}

function Dashboard({ faculty, pendingCount, cancelledCount, regularCount, makeupCount, timetable, notifications, loading, onNavigate, onGenerate }) {
  const upcoming = [...timetable].sort(sortSessions).slice(0, 4);
  return (
    <>
      <div className="hero-card">
        <div>
          <div className="hero-kicker">FACULTY DASHBOARD</div>
          <h2>Good morning, {faculty.name}.</h2>

          <p>
            Teaching: <strong>{faculty.subjects}</strong>
          </p>

          <p>
            Manage classes, released slots, rooms and makeup scheduling from one place.
          </p>
          <div className="hero-actions">
            <button className="primary-btn" onClick={onGenerate}>Generate Timetable</button>
            <button className="ghost-btn" onClick={() => onNavigate('notifications')}>View Notifications</button>
          </div>
        </div>
        <div className="hero-illustration">
          <div className="orbit orbit-one" />
          <div className="orbit orbit-two" />
          <div className="hero-orb">UT</div>
        </div>
      </div>

      <div className="stats-grid">
        <StatCard label="Scheduled Classes" value={regularCount} trend="Live from MySQL" tone="blue" />
        <StatCard label="Makeup Classes" value={makeupCount} trend="Accepted sessions" tone="purple" />
        <StatCard label="Pending Alerts" value={pendingCount} trend="Needs attention" tone="amber" />
        <StatCard label="Cancelled" value={cancelledCount} trend="Released slots" tone="red" />
      </div>

      <div className="dashboard-grid">
        <Card title="Upcoming timetable" action={<button className="link-btn" onClick={() => onNavigate('timetable')}>View all →</button>}>
          <div className="mini-table">
            <div className="mini-row mini-head"><span>Course</span><span>Time</span><span>Room</span></div>
            {upcoming.length === 0 && <Empty text="No scheduled classes yet." />}
            {upcoming.map(s => (
              <div className="mini-row" key={s.session_id}>
                <span><strong>{s.course_name}</strong><small>{s.session_type}</small></span>
                <span>{formatTime(s.start_time)}–{formatTime(s.end_time)}</span>
                <span>{s.room_number}</span>
              </div>
            ))}
          </div>
        </Card>

        <Card title="Latest notifications" action={<button className="link-btn" onClick={() => onNavigate('notifications')}>Open inbox →</button>}>
          <div className="notification-feed">
            {notifications.filter(n => n.response_status === 'PENDING').slice(0, 3).map(n => (
              <div className="feed-item" key={n.notification_id}>
                <div className="feed-dot" />
                <div>
                  <strong>Available slot</strong>
                  <p>{n.message}</p>
                </div>
                <span className="pending-badge">Pending</span>
              </div>
            ))}
            {!notifications.filter(n => n.response_status === 'PENDING').length && <Empty text="You're all caught up." />}
          </div>
        </Card>
      </div>
    </>
  );
}

function TimetablePage({ timetable, facultyList, loading, onGenerate }) {
  const sorted = [...timetable].sort(sortSessions);
  return (
    <div className="page-stack">
      <div className="page-toolbar">
        <div>
          <p className="muted">Conflict-free sessions returned by the scheduler.</p>
        </div>
        <button className="primary-btn" onClick={onGenerate}>{loading ? 'Generating…' : 'Generate Timetable'}</button>
      </div>
      <Card>
        <div className="data-table-wrap">
          <table className="data-table">
            <thead><tr><th>Session</th><th>Course</th><th>Faculty / Subject</th><th>Panel</th><th>Room</th><th>Type</th><th>Day</th><th>Time</th></tr></thead>
            <tbody>
              {sorted.map(s => (
                <tr key={s.session_id}>
                  <td className="mono">#{s.session_id}</td>
                  <td><strong>{s.course_name}</strong></td>
                  <td>
                    <strong>{s.faculty_name}</strong>{' '}
                    <small>
                      {facultyList.find(f => f.faculty_id === s.faculty_id)?.subjects || ''}
                    </small>
                  </td>
                  <td>{s.panel_name}</td>
                  <td>{s.room_number}</td>
                  <td><TypeBadge value={s.session_type} /></td>
                  <td>{s.day_of_week}</td>
                  <td>{formatTime(s.start_time)} – {formatTime(s.end_time)}</td>
                </tr>
              ))}
              {!sorted.length && <tr><td colSpan="8"><Empty text="No timetable data." /></td></tr>}
            </tbody>
          </table>
        </div>
      </Card>
    </div>
  );
}

function NotificationsPage({ notifications, onAccept, onDecline }) {
  return (
    <div className="page-stack">
      <div className="page-toolbar"><p className="muted">Available-slot alerts are targeted to faculty assigned to the relevant panel.</p></div>
      <div className="notification-grid">
        {notifications.map(n => (
          <Card key={n.notification_id} className={`notification-card ${n.response_status.toLowerCase()}`}>
            <div className="notification-top"><span className="eyebrow">NOTIFICATION #{n.notification_id}</span><StatusBadge value={n.response_status} /></div>
            <h3>Class slot available</h3>
            <p>{n.message}</p>
            <div className="notification-meta"><span>Session #{n.session_id}</span><span>{n.created_at || 'Just now'}</span></div>
            {n.response_status === 'PENDING' ? (
              <div className="button-row">
                <button className="primary-btn" onClick={() => onAccept(n)}>Accept Class</button>
                <button className="danger-ghost-btn" onClick={() => onDecline(n.notification_id)}>Decline</button>
              </div>
            ) : <div className="resolved-line">This notification has been handled.</div>}
          </Card>
        ))}
        {!notifications.length && <Card><Empty text="No notifications yet." /></Card>}
      </div>
    </div>
  );
}

function CancelPage({ classes, onCancel }) {
  return (
    <div className="page-stack">
      <div className="info-banner"><span>i</span><div><strong>Only your scheduled classes can be cancelled.</strong><p>Cancellation releases the slot and can trigger notifications for other faculty.</p></div></div>
      <div className="class-grid">
        {classes.map(s => (
          <Card key={s.session_id}>
            <div className="class-header"><div><span className="eyebrow">SESSION #{s.session_id}</span><h3>{s.course_name}</h3></div><TypeBadge value={s.session_type} /></div>
            <div className="class-details">
              <span><b>Panel</b>{s.panel_name}</span>
              <span><b>Room</b>{s.room_number}</span>
              <span><b>Time</b>{s.day_of_week}, {formatTime(s.start_time)}–{formatTime(s.end_time)}</span>
            </div>
            <button className="danger-btn" onClick={() => onCancel(s.session_id)}>Cancel class</button>
          </Card>
        ))}
        {!classes.length && <Card><Empty text="No scheduled classes available for cancellation." /></Card>}
      </div>
    </div>
  );
}

function RoomsPage({ slots, selectedSlot, setSelectedSlot, rooms, onSearch }) {
  return (
    <div className="page-stack">
      <Card>
        <div className="room-search-bar">
          <div><span className="eyebrow">LINEAR SEARCH</span><h3>Find an available room</h3><p className="muted">Select a slot and search rooms against scheduled sessions.</p></div>
          <div className="room-controls"><select value={selectedSlot} onChange={e => setSelectedSlot(Number(e.target.value))}>{slots.map(s => <option key={s.slot_id} value={s.slot_id}>{s.slot_id} · {s.day_of_week} · {formatTime(s.start_time)}–{formatTime(s.end_time)}</option>)}</select><button className="primary-btn" onClick={onSearch}>Search</button></div>
        </div>
      </Card>
      <div className="room-grid">
        {rooms.map(r => <div key={r.room_id} className={`room-tile ${r.available ? 'available' : 'occupied'}`}><div className="room-icon">⌂</div><div><strong>{r.room_number}</strong><span>{r.available ? 'Available' : 'Occupied'}</span></div><div className="room-state">{r.available ? '✓' : '×'}</div></div>)}
        {!rooms.length && <Card><Empty text="Run a search to see room availability." /></Card>}
      </div>
    </div>
  );
}

function MakeupsPage({ makeups }) {
  return (
    <div className="page-stack">
      <div className="page-toolbar"><p className="muted">Priority-backed makeup requests stored in MySQL.</p></div>
      <Card>
        <div className="data-table-wrap"><table className="data-table"><thead><tr><th>Request</th><th>Course</th><th>Priority</th><th>Status</th></tr></thead><tbody>{makeups.map(m => <tr key={m.request_id}><td className="mono">#{m.request_id}</td><td><strong>{m.course_name}</strong></td><td><span className="priority-pill">{m.priority}</span></td><td><StatusBadge value={m.status} /></td></tr>)}{!makeups.length && <tr><td colSpan="4"><Empty text="No makeup requests for this faculty." /></td></tr>}</tbody></table></div>
      </Card>
    </div>
  );
}

function NavItem({ active, label, icon, badge, onClick }) { return <button className={`nav-item ${active ? 'active' : ''}`} onClick={onClick}><span className="nav-icon">{icon}</span><span>{label}</span>{badge > 0 && <span className="nav-badge">{badge}</span>}</button>; }
function StatCard({ label, value, trend, tone }) { return <div className={`stat-card ${tone}`}><div className="stat-label">{label}</div><div className="stat-value">{value}</div><div className="stat-trend">{trend}</div></div>; }
function Card({ title, action, children, className='' }) { return <section className={`card ${className}`}>{(title || action) && <div className="card-head">{title && <h3>{title}</h3>}{action}</div>}{children}</section>; }
function Avatar({ name, small=false }) { const initials = name.split(' ').map(x => x[0]).join('').slice(0,2).toUpperCase(); return <div className={`avatar ${small ? 'small' : ''}`}>{initials}</div>; }
function TypeBadge({ value }) { return <span className={`type-badge ${String(value).toLowerCase()}`}>{value}</span>; }
function StatusBadge({ value }) { return <span className={`status-badge ${String(value).toLowerCase()}`}>{value}</span>; }
function Toast({ toast, onClose }) { return <div className={`toast ${toast.type}`}><span>{toast.type === 'success' ? '✓' : 'i'}</span><div>{toast.text}</div><button onClick={onClose}>×</button></div>; }
function Empty({ text }) { return <div className="empty">{text}</div>; }
function Modal({ title, onClose, children }) { return <div className="modal-backdrop" onMouseDown={onClose}><div className="modal" onMouseDown={e => e.stopPropagation()}><div className="modal-head"><h3>{title}</h3><button onClick={onClose}>×</button></div>{children}</div></div>; }

const API_BASE_URL =
    import.meta.env.VITE_API_BASE_URL || 'http://localhost:8000';

async function api(url, options = {}) {
  const res = await fetch(`${API_BASE_URL}${url}`, {
    headers: {
      'Content-Type': 'application/json',
      ...(options.headers || {})
    },
    ...options
  });

  if (!res.ok) {
    throw new Error(
        (await res.text()) || `HTTP ${res.status}`
    );
  }

  return res.json();
}

function pageTitle(page) {
  return ({ dashboard: 'Overview', timetable: 'My Timetable', notifications: 'Notifications', cancel: 'Cancel Class', rooms: 'Room Search', makeups: 'Makeup Requests' }[page]);
}
function formatTime(t='') { return String(t).slice(0,5); }
function sortSessions(a,b) { const day = { Monday:1, Tuesday:2, Wednesday:3, Thursday:4, Friday:5, Saturday:6, Sunday:7 }; return (day[a.day_of_week] || 99) - (day[b.day_of_week] || 99) || String(a.start_time).localeCompare(String(b.start_time)) || Number(a.session_id) - Number(b.session_id); }

createRoot(document.getElementById('root')).render(<App />);
