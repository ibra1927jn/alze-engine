// =============================================
// VALIDATION SERVICE TESTS
// =============================================
import { describe, it, expect } from 'vitest';
import {
    validateEmail,
    validatePhone,
    validateName,
    validateEmployeeId,
    validateUUID,
    validateHarnessId,
    validateNumberRange,
    validatePositiveInteger,
    sanitizeString,
    stripHtml,
    sanitizePhone,
    sanitizeName,
    validateFields,
} from './validation.service';

describe('Validation Service', () => {
    // =============================================
    // EMAIL VALIDATION
    // =============================================
    describe('validateEmail', () => {
        it('should validate correct email formats', () => {
            expect(validateEmail('test@example.com').valid).toBe(true);
            expect(validateEmail('user.name@domain.co.nz').valid).toBe(true);
            expect(validateEmail('user+tag@example.org').valid).toBe(true);
        });

        it('should reject invalid email formats', () => {
            expect(validateEmail('').valid).toBe(false);
            expect(validateEmail('notanemail').valid).toBe(false);
            expect(validateEmail('missing@domain').valid).toBe(false);
            expect(validateEmail('@nodomain.com').valid).toBe(false);
            expect(validateEmail('spaces in@email.com').valid).toBe(false);
        });

        it('should reject emails that are too long', () => {
            const longEmail = 'a'.repeat(250) + '@test.com';
            expect(validateEmail(longEmail).valid).toBe(false);
        });
    });

    // =============================================
    // PHONE VALIDATION
    // =============================================
    describe('validatePhone', () => {
        it('should validate international phone numbers', () => {
            expect(validatePhone('+64211234567').valid).toBe(true);
            expect(validatePhone('0211234567').valid).toBe(true);
            expect(validatePhone('+1234567890').valid).toBe(true);
        });

        it('should validate NZ phone numbers when required', () => {
            expect(validatePhone('+64211234567', true).valid).toBe(true);
            expect(validatePhone('0211234567', true).valid).toBe(true);
            expect(validatePhone('021 123 4567', true).valid).toBe(true);
        });

        it('should reject invalid phone numbers', () => {
            expect(validatePhone('').valid).toBe(false);
            expect(validatePhone('123').valid).toBe(false);
            expect(validatePhone('abc').valid).toBe(false);
        });
    });

    // =============================================
    // NAME VALIDATION
    // =============================================
    describe('validateName', () => {
        it('should validate correct names', () => {
            expect(validateName('John Smith').valid).toBe(true);
            expect(validateName("Mary O'Connor").valid).toBe(true);
            expect(validateName('José García').valid).toBe(true);
            expect(validateName('Anne-Marie').valid).toBe(true);
        });

        it('should reject invalid names', () => {
            expect(validateName('').valid).toBe(false);
            expect(validateName('A').valid).toBe(false);
            expect(validateName('Name123').valid).toBe(false);
            expect(validateName('Name<script>').valid).toBe(false);
        });

        it('should reject names that are too long', () => {
            const longName = 'A'.repeat(101);
            expect(validateName(longName).valid).toBe(false);
        });
    });

    // =============================================
    // ID VALIDATION
    // =============================================
    describe('validateEmployeeId', () => {
        it('should validate correct employee ID formats', () => {
            expect(validateEmployeeId('EMP-12345').valid).toBe(true);
            expect(validateEmployeeId('PK-001').valid).toBe(true);
            expect(validateEmployeeId('TEAM123').valid).toBe(true);
        });

        it('should reject invalid employee IDs', () => {
            expect(validateEmployeeId('').valid).toBe(false);
            expect(validateEmployeeId('123').valid).toBe(false);
            expect(validateEmployeeId('A-1').valid).toBe(false);
        });
    });

    describe('validateUUID', () => {
        it('should validate correct UUIDs', () => {
            expect(validateUUID('550e8400-e29b-41d4-a716-446655440000').valid).toBe(true);
            expect(validateUUID('6ba7b810-9dad-11d1-80b4-00c04fd430c8').valid).toBe(true);
        });

        it('should reject invalid UUIDs', () => {
            expect(validateUUID('').valid).toBe(false);
            expect(validateUUID('not-a-uuid').valid).toBe(false);
            expect(validateUUID('12345').valid).toBe(false);
        });
    });

    describe('validateHarnessId', () => {
        it('should validate correct harness IDs', () => {
            expect(validateHarnessId('H1').valid).toBe(true);
            expect(validateHarnessId('HARNESS01').valid).toBe(true);
            expect(validateHarnessId('A1B2C3').valid).toBe(true);
        });

        it('should reject invalid harness IDs', () => {
            expect(validateHarnessId('').valid).toBe(false);
            expect(validateHarnessId('H').valid).toBe(false);
            expect(validateHarnessId('TOO-LONG-ID').valid).toBe(false);
        });
    });

    // =============================================
    // NUMBER VALIDATION
    // =============================================
    describe('validateNumberRange', () => {
        it('should validate numbers within range', () => {
            expect(validateNumberRange(5, 1, 10).valid).toBe(true);
            expect(validateNumberRange(1, 1, 10).valid).toBe(true);
            expect(validateNumberRange(10, 1, 10).valid).toBe(true);
        });

        it('should reject numbers outside range', () => {
            expect(validateNumberRange(0, 1, 10).valid).toBe(false);
            expect(validateNumberRange(11, 1, 10).valid).toBe(false);
            expect(validateNumberRange(-5, 1, 10).valid).toBe(false);
        });
    });

    describe('validatePositiveInteger', () => {
        it('should validate positive integers', () => {
            expect(validatePositiveInteger(0).valid).toBe(true);
            expect(validatePositiveInteger(1).valid).toBe(true);
            expect(validatePositiveInteger(100).valid).toBe(true);
        });

        it('should reject non-positive integers', () => {
            expect(validatePositiveInteger(-1).valid).toBe(false);
            expect(validatePositiveInteger(1.5).valid).toBe(false);
            expect(validatePositiveInteger(NaN).valid).toBe(false);
        });
    });

    // =============================================
    // SANITIZATION
    // =============================================
    describe('sanitizeString', () => {
        it('should escape HTML entities', () => {
            expect(sanitizeString('<script>alert("xss")</script>')).toBe(
                '&lt;script&gt;alert(&quot;xss&quot;)&lt;&#x2F;script&gt;'
            );
        });

        it('should handle empty and null inputs', () => {
            expect(sanitizeString('')).toBe('');
            expect(sanitizeString(null as unknown as string)).toBe('');
        });

        it('should trim whitespace', () => {
            expect(sanitizeString('  hello  ')).toBe('hello');
        });
    });

    describe('stripHtml', () => {
        it('should remove HTML tags', () => {
            expect(stripHtml('<p>Hello <b>World</b></p>')).toBe('Hello World');
            expect(stripHtml('<script>evil()</script>text')).toBe('evil()text');
        });
    });

    describe('sanitizePhone', () => {
        it('should clean phone numbers', () => {
            expect(sanitizePhone('+64 21 123 4567')).toBe('+64211234567');
            expect(sanitizePhone('021-123-4567')).toBe('0211234567');
        });
    });

    describe('sanitizeName', () => {
        it('should normalize spaces in names', () => {
            expect(sanitizeName('  John    Smith  ')).toBe('John Smith');
        });
    });

    // =============================================
    // BATCH VALIDATION
    // =============================================
    describe('validateFields', () => {
        it('should validate multiple fields', () => {
            const result = validateFields([
                { field: 'email', value: 'test@example.com', validator: validateEmail as (value: unknown) => import('./validation.service').ValidationResult },
                { field: 'name', value: 'John Smith', validator: validateName as (value: unknown) => import('./validation.service').ValidationResult },
            ]);
            expect(result.valid).toBe(true);
            expect(Object.keys(result.errors)).toHaveLength(0);
        });

        it('should collect all errors', () => {
            const result = validateFields([
                { field: 'email', value: 'invalid', validator: validateEmail as (value: unknown) => import('./validation.service').ValidationResult },
                { field: 'name', value: 'A', validator: validateName as (value: unknown) => import('./validation.service').ValidationResult },
            ]);
            expect(result.valid).toBe(false);
            expect(result.errors.email).toBeDefined();
            expect(result.errors.name).toBeDefined();
        });
    });
});
