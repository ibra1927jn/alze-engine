/**
 * Input Sanitization Utilities
 * Prevent XSS, SQL injection, and other input-based attacks.
 * Use these on ALL user inputs before processing or storing.
 */

/**
 * Strip HTML tags from user input to prevent XSS.
 * Preserves the text content, removes all HTML/script tags.
 */
export function sanitizeHtml(input: string): string {
    return input
        .replace(/<script\b[^<]*(?:(?!<\/script>)<[^<]*)*<\/script>/gi, '')
        .replace(/<[^>]*>/g, '')
        .trim();
}

/**
 * Sanitize text input — strip dangerous characters.
 * For names, descriptions, notes, etc.
 */
export function sanitizeText(input: string, maxLength = 500): string {
    return sanitizeHtml(input)
        .replace(/[<>"'`;]/g, '') // Remove dangerous chars
        .substring(0, maxLength)
        .trim();
}

/**
 * Sanitize numeric input — ensure it's a valid number within range.
 */
export function sanitizeNumber(
    input: string | number,
    min = 0,
    max = 999999
): number {
    const num = typeof input === 'string' ? parseFloat(input) : input;
    if (isNaN(num)) return min;
    return Math.max(min, Math.min(max, num));
}

/**
 * Sanitize email — basic validation + lowercase.
 */
export function sanitizeEmail(input: string): string {
    return input.toLowerCase().trim().substring(0, 254);
}

/**
 * Sanitize search query — prevent regex injection.
 */
export function sanitizeSearchQuery(input: string): string {
    return input
        .replace(/[.*+?^${}()|[\]\\]/g, '\\$&') // Escape regex chars
        .substring(0, 100)
        .trim();
}

/**
 * Sanitize filename — only allow safe characters.
 */
export function sanitizeFilename(input: string): string {
    return input
        .replace(/[^a-zA-Z0-9._-]/g, '_')
        .substring(0, 255);
}
