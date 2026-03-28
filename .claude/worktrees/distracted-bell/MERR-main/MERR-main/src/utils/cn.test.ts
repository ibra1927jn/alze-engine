import { describe, it, expect } from 'vitest';
import { cn } from './cn';

describe('cn — Tailwind class merge utility', () => {
    it('merges simple class strings', () => {
        expect(cn('rounded-lg', 'p-4')).toBe('rounded-lg p-4');
    });

    it('handles conditional classes (falsy values)', () => {
        expect(cn('bg-white', false && 'text-red', undefined, null, 'p-2'))
            .toBe('bg-white p-2');
    });

    it('resolves Tailwind conflicts (last wins)', () => {
        // tailwind-merge should keep only the last padding
        expect(cn('p-4', 'p-2')).toBe('p-2');
    });

    it('resolves color conflicts', () => {
        expect(cn('bg-red-500', 'bg-blue-500')).toBe('bg-blue-500');
    });

    it('merges arrays of classes', () => {
        expect(cn(['rounded', 'shadow'], 'p-4')).toBe('rounded shadow p-4');
    });

    it('returns empty string for no input', () => {
        expect(cn()).toBe('');
    });

    it('handles complex conditional expressions', () => {
        const isActive = true;
        const isDisabled = false;
        const result = cn(
            'base-class',
            isActive && 'ring-2 ring-primary',
            isDisabled && 'opacity-50 cursor-not-allowed'
        );
        expect(result).toContain('base-class');
        expect(result).toContain('ring-2');
        expect(result).not.toContain('opacity-50');
    });
});
