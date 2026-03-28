/**
 * DocumentsTab.tsx — HR Document Management
 * Upload area + document templates list
 */
import React from 'react';

const DOCUMENT_TEMPLATES = [
    'Employment Agreement',
    'Work Visa',
    'Health & Safety Certificate',
    'Tax Declaration',
    'RSE Worker Induction',
    'Pastoral Care Plan',
];

const DocumentsTab: React.FC = () => (
    <div className="space-y-4">
        {/* Coming Soon Banner */}
        <div className="bg-amber-50 border border-amber-200 rounded-xl px-4 py-3 flex items-center gap-3">
            <span className="material-symbols-outlined text-amber-600">construction</span>
            <p className="text-sm font-medium text-amber-800">
                Coming Soon — Document upload and storage integration is under development.
            </p>
        </div>

        <div className="bg-white rounded-xl p-8 shadow-sm border border-border-light text-center">
            <span className="material-symbols-outlined text-text-disabled text-5xl mb-3 block">cloud_upload</span>
            <h3 className="font-bold text-text-primary mb-1">Document Management</h3>
            <p className="text-sm text-text-secondary mb-4">Upload and manage employment documents, visa copies, and certificates</p>
            <button
                disabled
                className="px-6 py-2.5 bg-indigo-600/50 text-white rounded-xl font-bold text-sm cursor-not-allowed"
                title="Upload functionality coming soon"
            >
                Upload Document
            </button>
        </div>

        <div className="grid grid-cols-1 lg:grid-cols-2 gap-3">
            {DOCUMENT_TEMPLATES.map((doc, i) => (
                <div key={i} className="bg-white rounded-xl p-4 shadow-sm border border-border-light flex items-center gap-3 hover:shadow-md transition-shadow cursor-pointer">
                    <div className="size-10 rounded-lg bg-surface-secondary flex items-center justify-center">
                        <span className="material-symbols-outlined text-text-secondary">description</span>
                    </div>
                    <div className="flex-1">
                        <h4 className="font-bold text-text-primary text-sm">{doc}</h4>
                        <p className="text-xs text-text-secondary">Template • Required for all employees</p>
                    </div>
                    <span className="material-symbols-outlined text-text-disabled hover:text-text-secondary transition-colors">download</span>
                </div>
            ))}
        </div>
    </div>
);

export default DocumentsTab;
