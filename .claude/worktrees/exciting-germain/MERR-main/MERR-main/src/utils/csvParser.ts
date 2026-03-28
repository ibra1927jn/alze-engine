/**
 * CSV Parser Utility
 *
 * Parses CSV files containing picker data for bulk import.
 * Uses papaparse for robust handling of UTF-8, BOM, quoted fields.
 */

import Papa from 'papaparse';

// ============================================
// TYPES
// ============================================

export interface CSVPickerRow {
    name: string;
    email?: string;
    phone?: string;
    picker_id?: string;
}

export interface ValidationError {
    row: number;
    field: string;
    message: string;
}

export interface DuplicateEntry {
    row: number;
    picker_id: string;
    existingName: string;
}

export interface ParseResult {
    valid: CSVPickerRow[];
    errors: ValidationError[];
    duplicates: DuplicateEntry[];
    totalRows: number;
}

// ============================================
// COLUMN MAPPING
// ============================================

/** Map various column header spellings to our canonical fields */
const COLUMN_ALIASES: Record<string, keyof CSVPickerRow> = {
    // Name
    'name': 'name',
    'nombre': 'name',
    'full_name': 'name',
    'fullname': 'name',
    'full name': 'name',
    'worker': 'name',
    'worker name': 'name',
    'picker': 'name',
    'picker name': 'name',

    // Email
    'email': 'email',
    'e-mail': 'email',
    'correo': 'email',
    'email address': 'email',

    // Phone
    'phone': 'phone',
    'telephone': 'phone',
    'tel': 'phone',
    'mobile': 'phone',
    'celular': 'phone',
    'phone number': 'phone',

    // Picker ID
    'picker_id': 'picker_id',
    'pickerid': 'picker_id',
    'id': 'picker_id',
    'badge': 'picker_id',
    'badge_id': 'picker_id',
    'employee_id': 'picker_id',
    'employee id': 'picker_id',
    'sticker': 'picker_id',
    'sticker_id': 'picker_id',
};

// ============================================
// VALIDATION
// ============================================

function validateRow(row: CSVPickerRow, index: number): ValidationError[] {
    const errors: ValidationError[] = [];

    // Name is required
    if (!row.name || row.name.trim().length === 0) {
        errors.push({ row: index + 1, field: 'name', message: 'Name is required' });
    } else if (row.name.trim().length < 2) {
        errors.push({ row: index + 1, field: 'name', message: 'Name must be at least 2 characters' });
    }

    // Email format (optional but validates if present)
    if (row.email && !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(row.email)) {
        errors.push({ row: index + 1, field: 'email', message: `Invalid email: ${row.email}` });
    }

    // Phone format (optional, basic NZ check)
    if (row.phone) {
        const cleaned = row.phone.replace(/[\s\-()]/g, '');
        if (cleaned.length < 7 || cleaned.length > 15) {
            errors.push({ row: index + 1, field: 'phone', message: `Invalid phone: ${row.phone}` });
        }
    }

    return errors;
}

// ============================================
// DUPLICATE DETECTION
// ============================================

interface ExistingPicker {
    picker_id: string;
    name: string;
}

function detectDuplicates(
    rows: CSVPickerRow[],
    existingPickers: ExistingPicker[]
): DuplicateEntry[] {
    const duplicates: DuplicateEntry[] = [];
    const existingIds = new Map(
        existingPickers.map(p => [p.picker_id?.toLowerCase(), p.name])
    );

    // 🔧 L30: Also detect duplicates WITHIN the CSV file itself
    // Without this, intra-file dupes pass validation but cause 23505 on bulk insert
    const seenInFile = new Map<string, number>(); // picker_id → first row number

    rows.forEach((row, index) => {
        if (row.picker_id) {
            const normalizedId = row.picker_id.toLowerCase();

            // Check against existing DB records
            const existing = existingIds.get(normalizedId);
            if (existing) {
                duplicates.push({
                    row: index + 1,
                    picker_id: row.picker_id,
                    existingName: existing
                });
                return;
            }

            // Check against earlier rows in same CSV
            const firstRow = seenInFile.get(normalizedId);
            if (firstRow !== undefined) {
                duplicates.push({
                    row: index + 1,
                    picker_id: row.picker_id,
                    existingName: `(duplicate of CSV row ${firstRow})`
                });
            } else {
                seenInFile.set(normalizedId, index + 1);
            }
        }
    });

    return duplicates;
}

// ============================================
// MAIN PARSER
// ============================================

export function parseCSV(
    file: File,
    existingPickers: ExistingPicker[] = []
): Promise<ParseResult> {
    return new Promise((resolve, reject) => {
        Papa.parse(file, {
            header: true,
            skipEmptyLines: true,
            transformHeader: (header: string) => {
                // Normalize header: lowercase, trim
                const normalized = header.toLowerCase().trim();
                return COLUMN_ALIASES[normalized] || normalized;
            },
            complete: (results) => {
                const allErrors: ValidationError[] = [];

                // Map raw rows to typed rows
                const mappedRows: CSVPickerRow[] = (results.data as Record<string, string>[]).map(
                    (raw, index) => {
                        const row: CSVPickerRow = {
                            name: (raw.name || '').trim(),
                            email: (raw.email || '').trim() || undefined,
                            phone: (raw.phone || '').trim() || undefined,
                            picker_id: (raw.picker_id || '').trim() || undefined,
                        };

                        // Validate each row
                        const rowErrors = validateRow(row, index);
                        allErrors.push(...rowErrors);

                        return row;
                    }
                );

                // Split into valid and invalid
                const errorRowNumbers = new Set(allErrors.map(e => e.row));
                const valid = mappedRows.filter((_, i) => !errorRowNumbers.has(i + 1));

                // Detect duplicates against existing DB records
                const duplicates = detectDuplicates(valid, existingPickers);
                const duplicateRowNumbers = new Set(duplicates.map(d => d.row));

                // Remove duplicates from valid list
                const finalValid = valid.filter((_, i) => !duplicateRowNumbers.has(i + 1));

                resolve({
                    valid: finalValid,
                    errors: allErrors,
                    duplicates,
                    totalRows: mappedRows.length,
                });
            },
            error: (error: Error) => {
                reject(new Error(`CSV parse error: ${error.message}`));
            },
        });
    });
}

/**
 * Generate a sample CSV template for download
 */
export function generateCSVTemplate(): string {
    return 'Name,Email,Phone,PickerID\n"John Smith",john@example.com,+64211234567,P-001\n"Ana García",ana@example.com,+64229876543,P-002\n';
}
