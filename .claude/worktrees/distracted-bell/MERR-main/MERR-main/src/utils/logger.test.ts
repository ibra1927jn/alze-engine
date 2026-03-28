import { describe, it, expect, vi, beforeEach } from 'vitest';
import { logger } from './logger';

describe('logger — Structured logging utility', () => {
    beforeEach(() => {
        vi.restoreAllMocks();
    });

    describe('in development mode (default in test)', () => {
        it('logger.info writes to console.log with prefix', () => {
            const spy = vi.spyOn(console, 'log').mockImplementation(() => { });
            logger.info('test message', { key: 'value' });
            expect(spy).toHaveBeenCalledWith('[HarvestPro]', 'test message', { key: 'value' });
        });

        it('logger.warn writes to console.warn with prefix', () => {
            const spy = vi.spyOn(console, 'warn').mockImplementation(() => { });
            logger.warn('slow query');
            expect(spy).toHaveBeenCalledWith('[HarvestPro]', 'slow query');
        });

        it('logger.error writes to console.error with prefix', () => {
            const spy = vi.spyOn(console, 'error').mockImplementation(() => { });
            const error = new Error('test error');
            logger.error('Failed to sync', error);
            expect(spy).toHaveBeenCalledWith('[HarvestPro]', 'Failed to sync', error);
        });

        it('logger.debug writes to console.debug with prefix', () => {
            const spy = vi.spyOn(console, 'debug').mockImplementation(() => { });
            logger.debug('debug data', 42);
            expect(spy).toHaveBeenCalledWith('[HarvestPro]', 'debug data', 42);
        });
    });

    describe('logger.error always fires', () => {
        it('logs errors regardless of environment', () => {
            const spy = vi.spyOn(console, 'error').mockImplementation(() => { });
            logger.error('critical failure');
            expect(spy).toHaveBeenCalledOnce();
        });

        it('handles multiple arguments', () => {
            const spy = vi.spyOn(console, 'error').mockImplementation(() => { });
            logger.error('msg', 'extra info', 42, { detail: true });
            expect(spy).toHaveBeenCalledWith('[HarvestPro]', 'msg', 'extra info', 42, { detail: true });
        });
    });

    describe('log methods accept various argument types', () => {
        it('handles no arguments', () => {
            const spy = vi.spyOn(console, 'log').mockImplementation(() => { });
            logger.info();
            expect(spy).toHaveBeenCalledWith('[HarvestPro]');
        });

        it('handles numerical arguments', () => {
            const spy = vi.spyOn(console, 'log').mockImplementation(() => { });
            logger.info(123, 456);
            expect(spy).toHaveBeenCalledWith('[HarvestPro]', 123, 456);
        });

        it('handles null and undefined', () => {
            const spy = vi.spyOn(console, 'log').mockImplementation(() => { });
            logger.info(null, undefined);
            expect(spy).toHaveBeenCalledWith('[HarvestPro]', null, undefined);
        });
    });
});
