/** @type {import('tailwindcss').Config} */
export default {
    content: [
        "./index.html",
        "./src/**/*.{js,ts,jsx,tsx}",
        "./pages/**/*.{js,ts,jsx,tsx}",
        "./components/**/*.{js,ts,jsx,tsx}",
        "./hooks/**/*.{js,ts,jsx,tsx}",
        "./context/**/*.{js,ts,jsx,tsx}",
        "./services/**/*.{js,ts,jsx,tsx}",
    ],
    darkMode: "class",
    theme: {
        extend: {
            colors: {
                // Professional Indigo Palette
                "primary": "#4338ca",           // Indigo 700 — primary brand
                "primary-vibrant": "#6366f1",   // Indigo 500 — interactive
                "primary-dim": "#3730a3",       // Indigo 800 — hover
                "primary-dark": "#312e81",      // Indigo 900 — pressed
                "primary-light": "#eef2ff",     // Indigo 50  — tints
                // Surfaces
                "background-light": "#f8fafc",  // Slate 50
                "surface-white": "#ffffff",
                // Text
                "text-main": "#0f172a",         // Slate 900
                "text-sub": "#475569",          // Slate 600
                "text-muted": "#94a3b8",        // Slate 400
                // Borders
                "border-light": "#e2e8f0",      // Slate 200
                "border-subtle": "#f1f5f9",     // Slate 100
                // Status
                "warning": "#f59e0b",
                "bonus": "#fbbf24",
                "success": "#10b981",
                "danger": "#ef4444",
                "info": "#0ea5e9",
                "info-dim": "#0284c7",
                // Lilac / Login palette
                "lilac": "#a855f7",
                "lilac-light": "#c084fc",
                "lilac-dark": "#7c3aed",
                "lilac-glow": "#d8b4fe",
                "lilac-50": "#faf5ff",
            },
            fontFamily: {
                "display": ["Inter", "sans-serif"],
                "body": ["Inter", "sans-serif"],
                "mono": ["ui-monospace", "SFMono-Regular", "Menlo", "Monaco", "Consolas", "monospace"],
            },
            boxShadow: {
                "card": "0 1px 3px rgba(0, 0, 0, 0.04), 0 1px 2px rgba(0, 0, 0, 0.02)",
                "card-hover": "0 4px 12px rgba(0, 0, 0, 0.06), 0 2px 4px rgba(0, 0, 0, 0.03)",
                "nav": "0 -2px 10px rgba(0, 0, 0, 0.04)",
                "glow": "0 4px 14px rgba(99, 102, 241, 0.25)",
                "glow-success": "0 4px 14px rgba(16, 185, 129, 0.3)",
                "glow-info": "0 4px 14px rgba(14, 165, 233, 0.3)",
                "rugged": "0 2px 8px rgba(0, 0, 0, 0.12), 0 1px 3px rgba(0, 0, 0, 0.08)",
            },
            padding: {
                "safe-top": "env(safe-area-inset-top)",
                "safe-bottom": "env(safe-area-inset-bottom)",
            },
            backdropBlur: {
                "xs": "2px",
            },
            keyframes: {
                "pulse-glow": {
                    "0%, 100%": { opacity: 1 },
                    "50%": { opacity: 0.7, boxShadow: "0 0 20px rgba(99, 102, 241, 0.3)" },
                },
                "fade-in": {
                    from: { opacity: 0 },
                    to: { opacity: 1 },
                },
                "slide-up": {
                    from: { opacity: 0, transform: "translateY(16px)" },
                    to: { opacity: 1, transform: "translateY(0)" },
                },
                "slide-down": {
                    from: { opacity: 0, transform: "translateY(-12px)" },
                    to: { opacity: 1, transform: "translateY(0)" },
                },
                "scale-in": {
                    from: { opacity: 0, transform: "scale(0.92)" },
                    to: { opacity: 1, transform: "scale(1)" },
                },
                "breathe": {
                    "0%, 100%": { opacity: 1, boxShadow: "0 0 0 0 rgba(16, 185, 129, 0.3)" },
                    "50%": { opacity: 0.92, boxShadow: "0 0 12px 4px rgba(16, 185, 129, 0.15)" },
                },
                "count-up": {
                    "0%": { transform: "scale(1)" },
                    "40%": { transform: "scale(1.12)" },
                    "100%": { transform: "scale(1)" },
                },
                "shake": {
                    "0%, 100%": { transform: "translateX(0)" },
                    "10%, 30%, 50%, 70%, 90%": { transform: "translateX(-4px)" },
                    "20%, 40%, 60%, 80%": { transform: "translateX(4px)" },
                },
                "float": {
                    "0%, 100%": { transform: "translateY(0) rotate(0deg)", opacity: "0.6" },
                    "50%": { transform: "translateY(-20px) rotate(5deg)", opacity: "0.9" },
                },
                "glow-lilac": {
                    "0%, 100%": { boxShadow: "0 0 20px rgba(168, 85, 247, 0.3)" },
                    "50%": { boxShadow: "0 0 40px rgba(168, 85, 247, 0.6)" },
                },
            },
            animation: {
                "pulse-glow": "pulse-glow 2s ease-in-out infinite",
                "fade-in": "fade-in 0.35s ease-out both",
                "slide-up": "slide-up 0.4s cubic-bezier(0.16, 1, 0.3, 1) both",
                "slide-down": "slide-down 0.3s ease-out both",
                "scale-in": "scale-in 0.25s cubic-bezier(0.16, 1, 0.3, 1) both",
                "breathe": "breathe 3s ease-in-out infinite",
                "count-up": "count-up 0.5s ease-out",
                "shake": "shake 0.5s cubic-bezier(.36,.07,.19,.97) both",
                "float": "float 6s ease-in-out infinite",
                "glow-lilac": "glow-lilac 3s ease-in-out infinite",
            },
        },
    },
    plugins: [
        require('@tailwindcss/forms'),
    ],
}
