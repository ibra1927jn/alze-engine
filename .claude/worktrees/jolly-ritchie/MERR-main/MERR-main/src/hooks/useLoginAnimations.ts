/**
 * useLoginAnimations — Animation hooks for the Login page
 *   useTypewriter  — Reveals text character by character
 *   useCounter     — Animated counter from 0 → target
 *   useParallax    — Mouse-based parallax on the hero panel
 */
import { useState, useEffect, useRef } from 'react';

/** Typewriter effect — reveals text character by character */
export const useTypewriter = (text: string, speed = 60, delay = 800) => {
    const [displayed, setDisplayed] = useState('');
    const [done, setDone] = useState(false);

    useEffect(() => {
        setDisplayed('');
        setDone(false);
        const timeout = setTimeout(() => {
            let i = 0;
            const interval = setInterval(() => {
                setDisplayed(text.slice(0, i + 1));
                i++;
                if (i >= text.length) {
                    clearInterval(interval);
                    setDone(true);
                }
            }, speed);
            return () => clearInterval(interval);
        }, delay);
        return () => clearTimeout(timeout);
    }, [text, speed, delay]);

    return { displayed, done };
};

/** Animated counter from 0 → target */
export const useCounter = (target: number, duration = 1800, delay = 1200) => {
    const [count, setCount] = useState(0);

    useEffect(() => {
        const timeout = setTimeout(() => {
            const startTime = performance.now();
            const animate = (now: number) => {
                const elapsed = now - startTime;
                const progress = Math.min(elapsed / duration, 1);
                const eased = 1 - Math.pow(1 - progress, 3);
                setCount(Math.round(eased * target));
                if (progress < 1) requestAnimationFrame(animate);
            };
            requestAnimationFrame(animate);
        }, delay);
        return () => clearTimeout(timeout);
    }, [target, duration, delay]);

    return count;
};

/** Mouse-based parallax on the hero panel */
export const useParallax = () => {
    const ref = useRef<HTMLDivElement>(null);
    const [offset, setOffset] = useState({ x: 0, y: 0 });

    useEffect(() => {
        const handleMouseMove = (e: MouseEvent) => {
            if (!ref.current) return;
            const rect = ref.current.getBoundingClientRect();
            const x = ((e.clientX - rect.left) / rect.width - 0.5) * 2;
            const y = ((e.clientY - rect.top) / rect.height - 0.5) * 2;
            setOffset({ x, y });
        };
        const el = ref.current;
        el?.addEventListener('mousemove', handleMouseMove);
        return () => el?.removeEventListener('mousemove', handleMouseMove);
    }, []);

    return { ref, offset };
};
