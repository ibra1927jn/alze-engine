/**
 * AddVehicleModal.tsx — Add new vehicle to fleet
 * Modal form for registering tractors/vehicles with name, plate, zone, driver
 */
import React, { useState } from 'react';
import ModalOverlay from '@/components/ui/ModalOverlay';

interface AddVehicleModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSubmit: (data: {
        name: string;
        plate: string;
        zone: string;
        driver_name: string;
        max_capacity: number;
    }) => void;
}

const ZONES = ['A1', 'A2', 'A3', 'A4', 'B1', 'B2', 'B3', 'C1'];

const AddVehicleModal: React.FC<AddVehicleModalProps> = ({ isOpen, onClose, onSubmit }) => {
    const [form, setForm] = useState({
        name: '',
        plate: '',
        zone: 'A1',
        driver_name: '',
        max_capacity: 20,
    });
    const [errors, setErrors] = useState<Record<string, string>>({});

    if (!isOpen) return null;

    const validate = () => {
        const errs: Record<string, string> = {};
        if (!form.name.trim()) errs.name = 'Vehicle name is required';
        if (!form.plate.trim()) errs.plate = 'Plate number is required';
        if (!form.driver_name.trim()) errs.driver_name = 'Driver name is required';
        if (form.max_capacity < 1 || form.max_capacity > 100) errs.max_capacity = 'Capacity must be 1-100';
        setErrors(errs);
        return Object.keys(errs).length === 0;
    };

    const handleSubmit = (e: React.FormEvent) => {
        e.preventDefault();
        if (!validate()) return;
        onSubmit(form);
        setForm({ name: '', plate: '', zone: 'A1', driver_name: '', max_capacity: 20 });
        onClose();
    };

    const update = (key: string, value: string | number) =>
        setForm(prev => ({ ...prev, [key]: value }));

    return (
        <ModalOverlay onClose={onClose}>
            {/* Header */}
            <div className="flex items-center justify-between px-5 py-4 border-b border-border-light">
                <div className="flex items-center gap-2">
                    <span className="material-symbols-outlined text-primary">agriculture</span>
                    <h2 className="text-base font-bold text-text-main">Add Vehicle</h2>
                </div>
                <button onClick={onClose} className="text-text-muted hover:text-text-main transition-colors">
                    <span className="material-symbols-outlined">close</span>
                </button>
            </div>

            {/* Form */}
            <form onSubmit={handleSubmit} className="p-5 space-y-4">
                {/* Name */}
                <div>
                    <label className="block text-xs font-bold text-text-sub mb-1">Vehicle Name</label>
                    <input
                        type="text"
                        placeholder="e.g. Tractor Alpha"
                        value={form.name}
                        onChange={e => update('name', e.target.value)}
                        className={`w-full px-3 py-2.5 rounded-xl border text-sm font-medium outline-none transition-all ${errors.name ? 'border-red-300 focus:border-red-500 focus:ring-red-500/10' : 'border-border-light focus:border-primary focus:ring-2 focus:ring-primary/10'
                            }`}
                    />
                    {errors.name && <p className="text-xs text-danger mt-1">{errors.name}</p>}
                </div>

                {/* Plate */}
                <div>
                    <label className="block text-xs font-bold text-text-sub mb-1">Plate Number</label>
                    <input
                        type="text"
                        placeholder="e.g. ABC123"
                        value={form.plate}
                        onChange={e => update('plate', e.target.value.toUpperCase())}
                        className={`w-full px-3 py-2.5 rounded-xl border text-sm font-medium outline-none transition-all ${errors.plate ? 'border-red-300 focus:border-red-500 focus:ring-red-500/10' : 'border-border-light focus:border-primary focus:ring-2 focus:ring-primary/10'
                            }`}
                    />
                    {errors.plate && <p className="text-xs text-danger mt-1">{errors.plate}</p>}
                </div>

                {/* Zone + Capacity row */}
                <div className="grid grid-cols-2 gap-3">
                    <div>
                        <label htmlFor="vehicle-zone" className="block text-xs font-bold text-text-sub mb-1">Zone</label>
                        <select
                            id="vehicle-zone"
                            value={form.zone}
                            onChange={e => update('zone', e.target.value)}
                            className="w-full px-3 py-2.5 rounded-xl border border-border-light text-sm font-medium outline-none focus:border-primary focus:ring-2 focus:ring-primary/10 bg-white transition-all"
                        >
                            {ZONES.map(z => <option key={z} value={z}>{z}</option>)}
                        </select>
                    </div>
                    <div>
                        <label htmlFor="vehicle-max-capacity" className="block text-xs font-bold text-text-sub mb-1">Max Capacity</label>
                        <input
                            id="vehicle-max-capacity"
                            type="number"
                            min={1}
                            max={100}
                            value={form.max_capacity}
                            onChange={e => update('max_capacity', parseInt(e.target.value) || 0)}
                            className={`w-full px-3 py-2.5 rounded-xl border text-sm font-medium outline-none transition-all ${errors.max_capacity ? 'border-red-300' : 'border-border-light focus:border-primary focus:ring-2 focus:ring-primary/10'
                                }`}
                        />
                        {errors.max_capacity && <p className="text-xs text-danger mt-1">{errors.max_capacity}</p>}
                    </div>
                </div>

                {/* Driver */}
                <div>
                    <label className="block text-xs font-bold text-text-sub mb-1">Driver Name</label>
                    <input
                        type="text"
                        placeholder="e.g. John Smith"
                        value={form.driver_name}
                        onChange={e => update('driver_name', e.target.value)}
                        className={`w-full px-3 py-2.5 rounded-xl border text-sm font-medium outline-none transition-all ${errors.driver_name ? 'border-red-300' : 'border-border-light focus:border-primary focus:ring-2 focus:ring-primary/10'
                            }`}
                    />
                    {errors.driver_name && <p className="text-xs text-danger mt-1">{errors.driver_name}</p>}
                </div>

                {/* Actions */}
                <div className="flex gap-2 pt-2">
                    <button
                        type="button"
                        onClick={onClose}
                        className="flex-1 py-2.5 rounded-xl border border-border-light text-sm font-bold text-text-sub hover:bg-slate-50 transition-colors"
                    >
                        Cancel
                    </button>
                    <button
                        type="submit"
                        className="flex-1 py-2.5 rounded-xl gradient-primary glow-primary text-white text-sm font-bold transition-all flex items-center justify-center gap-1.5 active:scale-95"
                    >
                        <span className="material-symbols-outlined text-sm">add</span>
                        Add Vehicle
                    </button>
                </div>
            </form>
        </ModalOverlay>
    );
};

export default AddVehicleModal;
