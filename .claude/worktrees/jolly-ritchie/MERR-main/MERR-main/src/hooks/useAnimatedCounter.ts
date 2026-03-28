/**
 * useAnimatedCounter — Smooth number counter animation hook
 * Extracted from DashboardView.tsx for reuse.
 * 
 * Uses ease-out cubic easing and requestAnimationFrame.
 * @param target - The number to animate towards
 * @param duration - Animation duration in ms (default: 1200)
 * @param delay - Delay before starting animation in ms (default: 0)
 */
import { useState, useEffect, useRef } from 'react';

export const useAnimatedCounter = (target: number, duration = 1200, delay = 0) => {
    const [count, setCount] = useState(0);
    const prevTarget = useRef(target);

    useEffect(() => {
        // Only animate when target changes meaningfully
        if (target === prevTarget.current && count !== 0) return;
        prevTarget.current = target;

        const timeout = setTimeout(() => {
            if (target === 0) { setCount(0); return; }
            const startTime = performance.now();
            const startVal = 0;
            const animate = (now: number) => {
                const elapsed = now - startTime;
                const progress = Math.min(elapsed / duration, 1);
                const eased = 1 - Math.pow(1 - progress, 3); // ease-out cubic
                setCount(Math.round(startVal + (target - startVal) * eased));
                if (progress < 1) requestAnimationFrame(animate);
            };
            requestAnimationFrame(animate);
        }, delay);
        return () => clearTimeout(timeout);
    }, [target, duration, delay]); // eslint-disable-line react-hooks/exhaustive-deps

    return count;
};
