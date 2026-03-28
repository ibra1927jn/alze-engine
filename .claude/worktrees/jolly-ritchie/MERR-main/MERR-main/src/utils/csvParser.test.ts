import { describe, it, expect, vi } from 'vitest';
import { parseCSV, generateCSVTemplate } from './csvParser';

// Helper to create a mock File from CSV string
function createCSVFile(content: string, name = 'test.csv'): File {
    return new File([content], name, { type: 'text/csv' });
}

describe('csvParser — Picker data import utility', () => {
    describe('parseCSV', () => {
        it('parses valid CSV with standard headers', async () => {
            const csv = 'Name,Email,Phone,PickerID\nJohn Smith,john@test.com,+64211234567,P-001\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.totalRows).toBe(1);
            expect(result.valid).toHaveLength(1);
            expect(result.errors).toHaveLength(0);
            expect(result.valid[0]).toEqual({
                name: 'John Smith',
                email: 'john@test.com',
                phone: '+64211234567',
                picker_id: 'P-001',
            });
        });

        it('handles column aliases (nombre, correo, celular)', async () => {
            const csv = 'Nombre,Correo,Celular\nJuan García,juan@test.com,+64219999999\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.valid).toHaveLength(1);
            expect(result.valid[0].name).toBe('Juan García');
            expect(result.valid[0].email).toBe('juan@test.com');
            expect(result.valid[0].phone).toBe('+64219999999');
        });

        it('handles case-insensitive headers', async () => {
            const csv = 'NAME,EMAIL,PHONE\nTest User,test@x.com,+64211111111\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.valid).toHaveLength(1);
            expect(result.valid[0].name).toBe('Test User');
        });

        it('validates required name field', async () => {
            const csv = 'Name,Email\n,missing@test.com\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.errors).toHaveLength(1);
            expect(result.errors[0].field).toBe('name');
            expect(result.errors[0].message).toBe('Name is required');
        });

        it('validates minimum name length', async () => {
            const csv = 'Name,Email\nA,a@test.com\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.errors).toHaveLength(1);
            expect(result.errors[0].message).toContain('at least 2 characters');
        });

        it('validates email format', async () => {
            const csv = 'Name,Email\nJohn Smith,not-an-email\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.errors).toHaveLength(1);
            expect(result.errors[0].field).toBe('email');
            expect(result.errors[0].message).toContain('Invalid email');
        });

        it('accepts valid emails', async () => {
            const csv = 'Name,Email\nJohn Smith,john@example.co.nz\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.errors).toHaveLength(0);
            expect(result.valid[0].email).toBe('john@example.co.nz');
        });

        it('validates phone number length', async () => {
            const csv = 'Name,Phone\nJohn Smith,123\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.errors).toHaveLength(1);
            expect(result.errors[0].field).toBe('phone');
        });

        it('accepts valid NZ phone numbers', async () => {
            const csv = 'Name,Phone\nJohn Smith,+64 21 123 4567\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.errors).toHaveLength(0);
        });

        it('optional fields can be empty', async () => {
            const csv = 'Name,Email,Phone,PickerID\nJohn Smith,,,\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.valid).toHaveLength(1);
            expect(result.valid[0].email).toBeUndefined();
            expect(result.valid[0].phone).toBeUndefined();
            expect(result.valid[0].picker_id).toBeUndefined();
        });

        it('skips empty rows', async () => {
            const csv = 'Name,Email\nJohn Smith,john@test.com\n\n\nJane Doe,jane@test.com\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.totalRows).toBe(2);
        });

        it('detects duplicates against existing pickers', async () => {
            const csv = 'Name,PickerID\nJohn Smith,P-001\nJane Doe,P-002\n';
            const existing = [{ picker_id: 'P-001', name: 'Existing John' }];
            const result = await parseCSV(createCSVFile(csv), existing);

            expect(result.duplicates).toHaveLength(1);
            expect(result.duplicates[0].picker_id).toBe('P-001');
            expect(result.duplicates[0].existingName).toBe('Existing John');
        });

        it('detects intra-file duplicates', async () => {
            const csv = 'Name,PickerID\nJohn Smith,P-001\nDuplicate John,P-001\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.duplicates).toHaveLength(1);
            expect(result.duplicates[0].existingName).toContain('duplicate of CSV row');
        });

        it('case-insensitive duplicate detection', async () => {
            const csv = 'Name,PickerID\nJohn Smith,p-001\n';
            const existing = [{ picker_id: 'P-001', name: 'Existing John' }];
            const result = await parseCSV(createCSVFile(csv), existing);

            expect(result.duplicates).toHaveLength(1);
        });

        it('handles multiple rows with mixed valid/invalid', async () => {
            const csv = 'Name,Email,Phone\nValid User,valid@test.com,+64211111111\n,bad@email,123\nGood Name,,\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.valid.length).toBeGreaterThanOrEqual(1);
            expect(result.errors.length).toBeGreaterThanOrEqual(1);
            expect(result.totalRows).toBe(3);
        });

        it('handles "Worker Name" and "Badge" as aliases', async () => {
            const csv = 'Worker Name,Badge\nTest Worker,B-100\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.valid[0].name).toBe('Test Worker');
            expect(result.valid[0].picker_id).toBe('B-100');
        });

        it('handles "Sticker_ID" as picker_id alias', async () => {
            const csv = 'Name,Sticker_ID\nSticker Worker,S-42\n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.valid[0].picker_id).toBe('S-42');
        });

        it('trims whitespace from values', async () => {
            const csv = 'Name,Email\n  Spaces User  ,  spaced@test.com  \n';
            const result = await parseCSV(createCSVFile(csv));

            expect(result.valid[0].name).toBe('Spaces User');
            expect(result.valid[0].email).toBe('spaced@test.com');
        });
    });

    describe('generateCSVTemplate', () => {
        it('returns a valid CSV template string', () => {
            const template = generateCSVTemplate();
            expect(template).toContain('Name,Email,Phone,PickerID');
        });

        it('contains example rows', () => {
            const template = generateCSVTemplate();
            expect(template).toContain('John Smith');
            expect(template).toContain('Ana García');
        });

        it('contains example phone numbers with NZ prefix', () => {
            const template = generateCSVTemplate();
            expect(template).toContain('+6421');
        });
    });
});
