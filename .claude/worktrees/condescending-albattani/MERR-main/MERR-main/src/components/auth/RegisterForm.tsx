import React, { useState, useRef } from 'react';

interface RegisterFormProps {
    fullName: string;
    setFullName: (v: string) => void;
    email: string;
    setEmail: (v: string) => void;
    password: string;
    setPassword: (v: string) => void;
    isSubmitting: boolean;
    onSubmit: (e: React.FormEvent) => void;
}

const RegisterForm: React.FC<RegisterFormProps> = ({
    fullName, setFullName, email, setEmail, password, setPassword,
    isSubmitting, onSubmit,
}) => {
    const [showPassword, setShowPassword] = useState(false);
    const [focusedField, setFocusedField] = useState<string | null>(null);
    const btnRef = useRef<HTMLButtonElement>(null);

    const handleBtnMouseMove = (e: React.MouseEvent<HTMLButtonElement>) => {
        if (!btnRef.current) return;
        const rect = btnRef.current.getBoundingClientRect();
        const x = ((e.clientX - rect.left) / rect.width) * 100;
        const y = ((e.clientY - rect.top) / rect.height) * 100;
        btnRef.current.style.setProperty('--ripple-x', `${x}%`);
        btnRef.current.style.setProperty('--ripple-y', `${y}%`);
    };

    const isFocused = (field: string) => focusedField === field;

    return (
        <form onSubmit={onSubmit} className="space-y-5">
            {/* HR Authorization Notice */}
            <div className="p-4 bg-lilac-50 border border-lilac/15 rounded-xl flex items-start gap-3 animate-slide-up">
                <span className="material-symbols-outlined text-lilac text-lg mt-0.5 animate-pulse">info</span>
                <p className="text-xs text-lilac-dark leading-relaxed">
                    Register with the <strong className="text-lilac-dark font-bold">email authorized by HR</strong>. Your role and department are assigned automatically.
                </p>
            </div>

            {/* Full Name */}
            <div>
                <label className={`text-xs font-semibold uppercase tracking-wider mb-2 block transition-colors duration-300 ${isFocused('name') ? 'text-lilac' : 'text-slate-500'}`}>
                    Full Name
                </label>
                <div className="relative group">
                    <span className={`material-symbols-outlined absolute left-4 top-1/2 -translate-y-1/2 text-lg transition-all duration-300 ${isFocused('name') ? 'text-lilac scale-110' : 'text-lilac-glow/60'}`}>
                        person
                    </span>
                    <input
                        type="text"
                        value={fullName}
                        onChange={(e) => setFullName(e.target.value)}
                        onFocus={() => setFocusedField('name')}
                        onBlur={() => setFocusedField(null)}
                        placeholder="John Doe"
                        className="w-full pl-12 pr-4 py-3.5 rounded-xl bg-white border-2 border-slate-200 focus:border-lilac focus:ring-4 focus:ring-lilac/10 text-slate-800 placeholder-slate-300 outline-none transition-all duration-300 font-medium"
                        required
                    />
                    <div className={`absolute bottom-0 left-1/2 h-[2px] bg-gradient-to-r from-lilac to-lilac-light rounded-full transition-all duration-500 ${isFocused('name') ? 'w-[calc(100%-2rem)] -translate-x-1/2' : 'w-0 -translate-x-1/2'}`} />
                </div>
            </div>

            {/* Email */}
            <div>
                <label className={`text-xs font-semibold uppercase tracking-wider mb-2 block transition-colors duration-300 ${isFocused('email') ? 'text-lilac' : 'text-slate-500'}`}>
                    Email
                </label>
                <div className="relative group">
                    <span className={`material-symbols-outlined absolute left-4 top-1/2 -translate-y-1/2 text-lg transition-all duration-300 ${isFocused('email') ? 'text-lilac scale-110' : 'text-lilac-glow/60'}`}>
                        mail
                    </span>
                    <input
                        type="email"
                        value={email}
                        onChange={(e) => setEmail(e.target.value)}
                        onFocus={() => setFocusedField('email')}
                        onBlur={() => setFocusedField(null)}
                        placeholder="you@email.com"
                        className="w-full pl-12 pr-4 py-3.5 rounded-xl bg-white border-2 border-slate-200 focus:border-lilac focus:ring-4 focus:ring-lilac/10 text-slate-800 placeholder-slate-300 outline-none transition-all duration-300 font-medium"
                        required
                    />
                    <div className={`absolute bottom-0 left-1/2 h-[2px] bg-gradient-to-r from-lilac to-lilac-light rounded-full transition-all duration-500 ${isFocused('email') ? 'w-[calc(100%-2rem)] -translate-x-1/2' : 'w-0 -translate-x-1/2'}`} />
                </div>
            </div>

            {/* Password */}
            <div>
                <label className={`text-xs font-semibold uppercase tracking-wider mb-2 block transition-colors duration-300 ${isFocused('pass') ? 'text-lilac' : 'text-slate-500'}`}>
                    Password
                </label>
                <div className="relative group">
                    <span className={`material-symbols-outlined absolute left-4 top-1/2 -translate-y-1/2 text-lg transition-all duration-300 ${isFocused('pass') ? 'text-lilac scale-110' : 'text-lilac-glow/60'}`}>
                        lock
                    </span>
                    <input
                        type={showPassword ? 'text' : 'password'}
                        value={password}
                        onChange={(e) => setPassword(e.target.value)}
                        onFocus={() => setFocusedField('pass')}
                        onBlur={() => setFocusedField(null)}
                        placeholder="Min. 6 characters"
                        className="w-full pl-12 pr-12 py-3.5 rounded-xl bg-white border-2 border-slate-200 focus:border-lilac focus:ring-4 focus:ring-lilac/10 text-slate-800 placeholder-slate-300 outline-none transition-all duration-300 font-medium"
                        required
                        minLength={6}
                    />
                    <button
                        type="button"
                        onClick={() => setShowPassword(!showPassword)}
                        className="absolute right-4 top-1/2 -translate-y-1/2 text-slate-300 hover:text-lilac hover:scale-110 transition-all duration-200"
                    >
                        <span className="material-symbols-outlined text-lg">{showPassword ? 'visibility_off' : 'visibility'}</span>
                    </button>
                    <div className={`absolute bottom-0 left-1/2 h-[2px] bg-gradient-to-r from-lilac to-lilac-light rounded-full transition-all duration-500 ${isFocused('pass') ? 'w-[calc(100%-2rem)] -translate-x-1/2' : 'w-0 -translate-x-1/2'}`} />
                </div>
            </div>

            {/* Submit Button — with magnetic hover glow */}
            <button
                ref={btnRef}
                type="submit"
                disabled={isSubmitting}
                onMouseMove={handleBtnMouseMove}
                className="login-btn-ripple w-full py-4 bg-gradient-to-r from-lilac to-lilac-dark hover:from-lilac-light hover:to-lilac text-white rounded-xl font-bold text-sm uppercase tracking-widest disabled:opacity-50 shadow-lg shadow-lilac/25 hover:shadow-2xl hover:shadow-lilac/40 hover:-translate-y-0.5 active:translate-y-0 active:scale-[0.97] transition-all duration-300"
            >
                {isSubmitting ? (
                    <span className="inline-flex items-center gap-2">
                        <span className="w-4 h-4 border-2 border-white/30 border-t-white rounded-full animate-spin" />
                        Creating account...
                    </span>
                ) : (
                    <span className="inline-flex items-center gap-2">
                        <span className="material-symbols-outlined text-base">person_add</span>
                        Create Account
                    </span>
                )}
            </button>
        </form>
    );
};

export default RegisterForm;
