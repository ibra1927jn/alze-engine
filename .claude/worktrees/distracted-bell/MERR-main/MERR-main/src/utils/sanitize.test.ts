/**
 * Tests for input sanitization utilities
 */
import { describe, it, expect } from 'vitest';
import {
    sanitizeHtml,
    sanitizeText,
    sanitizeNumber,
    sanitizeEmail,
    sanitizeSearchQuery,
    sanitizeFilename,
} from './sanitize';

describe('sanitizeHtml', () => {
    it('should strip script tags', () => {
        expect(sanitizeHtml('<script>alert("xss")</script>hello')).toBe('hello');
    });

    it('should strip HTML tags', () => {
        expect(sanitizeHtml('<b>bold</b> text')).toBe('bold text');
    });

    it('should preserve plain text', () => {
        expect(sanitizeHtml('Normal text here')).toBe('Normal text here');
    });

    it('should trim whitespace', () => {
        expect(sanitizeHtml('  text  ')).toBe('text');
    });

    it('should handle nested script tags', () => {
        expect(sanitizeHtml('<script>var x = "<script>";</script>Done')).toBe('Done');
    });
});

describe('sanitizeText', () => {
    it('should remove dangerous characters and HTML tags', () => {
        expect(sanitizeText('Hello <World>')).toBe('Hello');
    });

    it('should enforce max length', () => {
        const long = 'a'.repeat(600);
        expect(sanitizeText(long, 100).length).toBe(100);
    });

    it('should strip quotes and backticks', () => {
        expect(sanitizeText("it's a \"test\" `here`")).toBe('its a test here');
    });

    it('should use default max length of 500', () => {
        const long = 'b'.repeat(1000);
        expect(sanitizeText(long).length).toBe(500);
    });
});

describe('sanitizeNumber', () => {
    it('should parse valid number strings', () => {
        expect(sanitizeNumber('42')).toBe(42);
    });

    it('should clamp to min', () => {
        expect(sanitizeNumber('-5', 0, 100)).toBe(0);
    });

    it('should clamp to max', () => {
        expect(sanitizeNumber('200', 0, 100)).toBe(100);
    });

    it('should return min for NaN', () => {
        expect(sanitizeNumber('abc', 10, 100)).toBe(10);
    });

    it('should accept number type', () => {
        expect(sanitizeNumber(55, 0, 100)).toBe(55);
    });

    it('should handle float', () => {
        expect(sanitizeNumber('3.14', 0, 10)).toBeCloseTo(3.14);
    });
});

describe('sanitizeEmail', () => {
    it('should lowercase email', () => {
        expect(sanitizeEmail('User@Example.COM')).toBe('user@example.com');
    });

    it('should trim whitespace', () => {
        expect(sanitizeEmail('  user@test.com  ')).toBe('user@test.com');
    });

    it('should truncate at 254 chars', () => {
        const long = 'a'.repeat(300) + '@test.com';
        expect(sanitizeEmail(long).length).toBeLessThanOrEqual(254);
    });
});

describe('sanitizeSearchQuery', () => {
    it('should escape regex special chars', () => {
        expect(sanitizeSearchQuery('hello.*world')).toBe('hello\\.\\*world');
    });

    it('should truncate at 100 chars', () => {
        const long = 'x'.repeat(150);
        expect(sanitizeSearchQuery(long).length).toBe(100);
    });

    it('should preserve normal text', () => {
        expect(sanitizeSearchQuery('John Smith')).toBe('John Smith');
    });
});

describe('sanitizeFilename', () => {
    it('should replace spaces with underscores', () => {
        expect(sanitizeFilename('my file name.pdf')).toBe('my_file_name.pdf');
    });

    it('should remove special chars', () => {
        expect(sanitizeFilename('report<2024>.csv')).toBe('report_2024_.csv');
    });

    it('should truncate at 255 chars', () => {
        const long = 'f'.repeat(300) + '.txt';
        expect(sanitizeFilename(long).length).toBeLessThanOrEqual(255);
    });

    it('should allow dots, hyphens, underscores', () => {
        expect(sanitizeFilename('my-file_v2.0.csv')).toBe('my-file_v2.0.csv');
    });
});
