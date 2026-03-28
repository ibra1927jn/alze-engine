/**
 * CalendarTab.tsx — HR Calendar & Scheduling
 * Today's schedule + upcoming leave requests
 */
import React from 'react';

const TODAYS_SCHEDULE = [
    { time: '07:00', event: 'Shift Start — All Teams', icon: 'alarm', color: 'text-emerald-600' },
    { time: '10:00', event: 'New Employee Induction', icon: 'school', color: 'text-indigo-600' },
    { time: '12:00', event: 'Lunch Break (30min)', icon: 'restaurant', color: 'text-amber-600' },
    { time: '15:00', event: 'Safety Briefing — Zone B', icon: 'health_and_safety', color: 'text-red-600' },
    { time: '17:00', event: 'Shift End — Day Closure', icon: 'logout', color: 'text-text-secondary' },
];

const UPCOMING_LEAVE = [
    { name: 'Aroha W.', type: 'Annual Leave', dates: 'Feb 17-21', status: 'approved' },
    { name: 'Mateo S.', type: 'Sick Leave', dates: 'Feb 14', status: 'pending' },
];

const CalendarTab: React.FC = () => (
    <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
        {/* Preview Banner */}
        <div className="lg:col-span-2 bg-amber-50 border border-amber-200 rounded-xl px-4 py-3 flex items-center gap-3">
            <span className="material-symbols-outlined text-amber-600">construction</span>
            <p className="text-sm font-medium text-amber-800">
                Preview Only — Live calendar integration coming soon. Showing sample schedule data.
            </p>
        </div>

        {/* Today's Schedule */}
        <div className="bg-white rounded-xl p-5 shadow-sm border border-border-light">
            <h3 className="font-bold text-text-primary mb-4">Today's Schedule</h3>
            <div className="space-y-1">
                {TODAYS_SCHEDULE.map((item, i) => (
                    <div key={i} className="flex items-center gap-3 py-2.5 border-b border-border-light last:border-0">
                        <span className="text-xs font-mono text-text-muted w-12">{item.time}</span>
                        <span className={`material-symbols-outlined text-lg ${item.color}`}>{item.icon}</span>
                        <span className="text-sm text-text-primary font-medium">{item.event}</span>
                    </div>
                ))}
            </div>
        </div>

        {/* Upcoming Leave */}
        <div className="bg-white rounded-xl p-5 shadow-sm border border-border-light">
            <h3 className="font-bold text-text-primary mb-4">Upcoming Leave</h3>
            <div className="space-y-1">
                {UPCOMING_LEAVE.map((leave, i) => (
                    <div key={i} className="flex items-center justify-between py-2.5 border-b border-border-light last:border-0">
                        <div>
                            <p className="text-sm font-bold text-text-primary">{leave.name}</p>
                            <p className="text-xs text-text-secondary">{leave.type} • {leave.dates}</p>
                        </div>
                        <span className={`text-[10px] font-bold uppercase px-2 py-0.5 rounded-full ${leave.status === 'approved' ? 'bg-emerald-50 text-emerald-700' : 'bg-amber-50 text-amber-700'
                            }`}>{leave.status}</span>
                    </div>
                ))}
            </div>
        </div>
    </div>
);

export default CalendarTab;
