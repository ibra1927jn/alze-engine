/**
 * Barrel file for Team Leader Views
 * Correctly re-exporting default components
 */

export { default as HomeView } from './HomeView';
export { default as TeamView } from './TeamView';
export { default as LogisticsView } from './LogisticsView';
export { default as MessagingView } from './MessagingView';
export { default as ProfileView } from './ProfileView';
export { default as AttendanceView } from './AttendanceView';

// Si tienes vistas antiguas que aún no hemos migrado o borrado,
// puedes comentarlas o eliminarlas si ya no se usan en TeamLeader.tsx
// export { default as TasksView } from './TasksView';
// export { default as RunnersView } from './RunnersView';
