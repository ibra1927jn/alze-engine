// =============================================
// VALIDATION SERVICE - Input validation and sanitization
// =============================================

/**
 * Validation result with optional error message
 */
export interface ValidationResult {
    valid: boolean;
    error?: string;
}

/**
 * Validation rules for common field types
 */
export const ValidationRules = {
    EMAIL_REGEX: /^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$/,
    PHONE_REGEX: /^\+?[0-9]{7,15}$/,
    NZ_PHONE_REGEX: /^(?:\+?64|0)[2-9]\d{7,9}$/,
    NAME_REGEX: /^[a-zA-ZÀ-ÿ\s'-]{2,100}$/,
    EMPLOYEE_ID_REGEX: /^[A-Z]{2,4}-?\d{3,6}$/i,
    UUID_REGEX: /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i,
    HARNESS_ID_REGEX: /^[A-Z0-9]{2,10}$/i,
} as const;

// =============================================
// EMAIL VALIDATION
// =============================================

/**
 * Validate email address format
 */
export function validateEmail(email: string): ValidationResult {
    if (!email || typeof email !== 'string') {
        return { valid: false, error: 'Email is required' };
    }

    const trimmed = email.trim().toLowerCase();
    if (trimmed.length === 0) {
        return { valid: false, error: 'Email cannot be empty' };
    }

    if (trimmed.length > 254) {
        return { valid: false, error: 'Email is too long' };
    }

    if (!ValidationRules.EMAIL_REGEX.test(trimmed)) {
        return { valid: false, error: 'Invalid email format' };
    }

    return { valid: true };
}

// =============================================
// PHONE VALIDATION
// =============================================

/**
 * Validate phone number (international or NZ format)
 */
export function validatePhone(phone: string, requireNZ = false): ValidationResult {
    if (!phone || typeof phone !== 'string') {
        return { valid: false, error: 'Phone number is required' };
    }

    // Remove spaces and dashes for validation
    const cleaned = phone.replace(/[\s-]/g, '');

    if (requireNZ) {
        if (!ValidationRules.NZ_PHONE_REGEX.test(cleaned)) {
            return { valid: false, error: 'Invalid NZ phone number format' };
        }
    } else {
        if (!ValidationRules.PHONE_REGEX.test(cleaned)) {
            return { valid: false, error: 'Invalid phone number format' };
        }
    }

    return { valid: true };
}

// =============================================
// NAME VALIDATION
// =============================================

/**
 * Validate person name (allows letters, spaces, hyphens, apostrophes)
 */
export function validateName(name: string): ValidationResult {
    if (!name || typeof name !== 'string') {
        return { valid: false, error: 'Name is required' };
    }

    const trimmed = name.trim();
    if (trimmed.length < 2) {
        return { valid: false, error: 'Name must be at least 2 characters' };
    }

    if (trimmed.length > 100) {
        return { valid: false, error: 'Name is too long (max 100 characters)' };
    }

    if (!ValidationRules.NAME_REGEX.test(trimmed)) {
        return { valid: false, error: 'Name contains invalid characters' };
    }

    return { valid: true };
}

// =============================================
// ID VALIDATION
// =============================================

/**
 * Validate employee ID format (e.g., EMP-12345, PK-001)
 */
export function validateEmployeeId(id: string): ValidationResult {
    if (!id || typeof id !== 'string') {
        return { valid: false, error: 'Employee ID is required' };
    }

    const trimmed = id.trim().toUpperCase();
    if (!ValidationRules.EMPLOYEE_ID_REGEX.test(trimmed)) {
        return { valid: false, error: 'Invalid employee ID format (e.g., EMP-12345)' };
    }

    return { valid: true };
}

/**
 * Validate UUID format
 */
export function validateUUID(uuid: string): ValidationResult {
    if (!uuid || typeof uuid !== 'string') {
        return { valid: false, error: 'ID is required' };
    }

    if (!ValidationRules.UUID_REGEX.test(uuid.trim())) {
        return { valid: false, error: 'Invalid ID format' };
    }

    return { valid: true };
}

/**
 * Validate harness ID (alphanumeric, 2-10 chars)
 */
export function validateHarnessId(id: string): ValidationResult {
    if (!id || typeof id !== 'string') {
        return { valid: false, error: 'Harness ID is required' };
    }

    const trimmed = id.trim().toUpperCase();
    if (!ValidationRules.HARNESS_ID_REGEX.test(trimmed)) {
        return { valid: false, error: 'Invalid harness ID format (2-10 alphanumeric characters)' };
    }

    return { valid: true };
}

// =============================================
// NUMBER VALIDATION
// =============================================

/**
 * Validate number is within a range
 */
export function validateNumberRange(
    value: number,
    min: number,
    max: number,
    fieldName = 'Value'
): ValidationResult {
    if (typeof value !== 'number' || isNaN(value)) {
        return { valid: false, error: `${fieldName} must be a number` };
    }

    if (value < min) {
        return { valid: false, error: `${fieldName} must be at least ${min}` };
    }

    if (value > max) {
        return { valid: false, error: `${fieldName} must be at most ${max}` };
    }

    return { valid: true };
}

/**
 * Validate positive integer
 */
export function validatePositiveInteger(value: number, fieldName = 'Value'): ValidationResult {
    if (typeof value !== 'number' || isNaN(value)) {
        return { valid: false, error: `${fieldName} must be a number` };
    }

    if (!Number.isInteger(value) || value < 0) {
        return { valid: false, error: `${fieldName} must be a positive integer` };
    }

    return { valid: true };
}

// =============================================
// SANITIZATION
// =============================================

/**
 * Sanitize string to prevent XSS attacks
 * Removes or encodes potentially dangerous characters
 */
export function sanitizeString(input: string): string {
    if (!input || typeof input !== 'string') return '';

    return input
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;')
        .replace(/\//g, '&#x2F;')
        .trim();
}

/**
 * Sanitize HTML by removing all tags
 */
export function stripHtml(input: string): string {
    if (!input || typeof input !== 'string') return '';
    return input.replace(/<[^>]*>/g, '').trim();
}

/**
 * Sanitize for use in SQL-like contexts (additional protection layer)
 * Note: Always use parameterized queries, this is defense-in-depth
 */
export function sanitizeForQuery(input: string): string {
    if (!input || typeof input !== 'string') return '';

    return input
        .replace(/'/g, "''")
        .replace(/\\/g, '\\\\')
        .replace(/\0/g, '')
        .trim();
}

/**
 * Clean phone number to digits only (preserving + for international)
 */
export function sanitizePhone(phone: string): string {
    if (!phone) return '';
    return phone.replace(/[^\d+]/g, '');
}

/**
 * Clean name (trim whitespace, normalize spaces)
 */
export function sanitizeName(name: string): string {
    if (!name) return '';
    return name.trim().replace(/\s+/g, ' ');
}

// =============================================
// BATCH VALIDATION
// =============================================

export interface FieldValidation {
    field: string;
    value: unknown;
    validator: (value: unknown) => ValidationResult;
}

/**
 * Validate multiple fields at once
 */
export function validateFields(
    fields: FieldValidation[]
): { valid: boolean; errors: Record<string, string> } {
    const errors: Record<string, string> = {};

    for (const { field, value, validator } of fields) {
        const result = validator(value);
        if (!result.valid && result.error) {
            errors[field] = result.error;
        }
    }

    return {
        valid: Object.keys(errors).length === 0,
        errors,
    };
}

// =============================================
// EXPORTS
// =============================================

export const validationService = {
    // Validators
    validateEmail,
    validatePhone,
    validateName,
    validateEmployeeId,
    validateUUID,
    validateHarnessId,
    validateNumberRange,
    validatePositiveInteger,
    validateFields,

    // Sanitizers
    sanitizeString,
    stripHtml,
    sanitizeForQuery,
    sanitizePhone,
    sanitizeName,

    // Rules
    ValidationRules,
};

export default validationService;
