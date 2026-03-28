// =============================================
// ENGLISH TRANSLATIONS (Base)
// =============================================

export const translations: Record<string, string> = {
    // Common
    'common.loading': 'Loading...',
    'common.save': 'Save',
    'common.cancel': 'Cancel',
    'common.close': 'Close',
    'common.confirm': 'Confirm',
    'common.delete': 'Delete',
    'common.edit': 'Edit',
    'common.add': 'Add',
    'common.search': 'Search',
    'common.filter': 'Filter',
    'common.refresh': 'Refresh',
    'common.back': 'Back',
    'common.next': 'Next',
    'common.done': 'Done',
    'common.yes': 'Yes',
    'common.no': 'No',
    'common.ok': 'OK',
    'common.error': 'Error',
    'common.success': 'Success',
    'common.warning': 'Warning',

    // Navigation
    'nav.logistics': 'Logistics',
    'nav.runners': 'Runners',
    'nav.warehouse': 'Warehouse',
    'nav.messaging': 'Messaging',
    'nav.team': 'Team',
    'nav.rows': 'Rows',
    'nav.quality': 'Quality',
    'nav.settings': 'Settings',

    // Headers
    'header.logisticsHub': 'Logistics Hub',
    'header.orchardRunners': 'Orchard Runners',
    'header.warehouseInventory': 'Warehouse Inventory',
    'header.messagingHub': 'Messaging Hub',
    'header.teamManagement': 'Team Management',
    'header.rowAssignments': 'Row Assignments',

    // Offline Banner
    'offline.syncPending': 'Offline Sync Pending',
    'offline.updated': 'Updated {{time}} ago',

    // Logistics View
    'logistics.bucketsCollected': 'Buckets Collected',
    'logistics.full': 'Full',
    'logistics.binFull': 'Bin Full',
    'logistics.active': 'Active',
    'logistics.ready': 'Ready',
    'logistics.approachingLimit': '⚠️ Approaching 72-bucket limit',
    'logistics.prepareSwap': 'Prepare for bin swap',
    'logistics.limitReached': '🚫 LIMIT REACHED - DO NOT ADD MORE',
    'logistics.closeImmediately': 'Close bin immediately to prevent fruit damage',

    // Sun Exposure
    'sun.exposure': 'Sun Exposure',
    'sun.critical': '🚨 CRITICAL!',
    'sun.safeLevel': 'Safe Level',
    'sun.moveToShade': 'Move to shade!',

    // Supply Management
    'supply.management': 'Supply Management',
    'supply.emptyBins': 'Empty Bins',
    'supply.fullBins': 'Full Bins',
    'supply.low': '⚠️ Low',
    'supply.ok': 'OK',
    'supply.requestRefill': 'Request Refill',
    'supply.refillRequested': '📦 Refill requested!',
    'supply.binsEnRoute': '✅ {{count}} empty bins en route',
    'supply.eta': '🚛 ETA: {{minutes}} minutes from depot',

    // Runners
    'runners.active': 'Active Runners',
    'runners.addRunner': 'Add Runner',
    'runners.noActive': 'No Runners Active',
    'runners.addFirst': 'Add First Runner',
    'runners.addToTrack': 'Add runners to track their activity',
    'runners.manageRunner': 'Manage Runner',
    'runners.started': 'Started {{time}}',
    'runners.assignment': 'Assignment',
    'runners.noAssignment': 'No assignment',
    'runners.buckets': 'Buckets',
    'runners.bins': 'Bins',
    'runners.orchardMap': 'Orchard Map',
    'runners.gpsComingSoon': 'Real-time GPS tracking coming soon',

    // Warehouse
    'warehouse.harvestedStock': 'Harvested Stock',
    'warehouse.fullCherryBins': 'Full Cherry Bins',
    'warehouse.filled': 'filled',
    'warehouse.manualAdjustment': 'Manual Adjustment:',
    'warehouse.emptyBinsAvailable': 'Empty Bins Available',
    'warehouse.waitingTransport': 'Waiting Transport',
    'warehouse.critical': '🚨 CRITICAL: Empty bins depleted!',
    'warehouse.lowStock': '⚠️ Low stock alert',
    'warehouse.requestResupply': 'Request immediate resupply from depot',
    'warehouse.nextTruck': 'Next Resupply Truck',
    'warehouse.scheduledArrival': 'Scheduled arrival in {{minutes}} mins from Depot A',
    'warehouse.requestTransport': 'Request Transport',

    // Scanner
    'scanner.scanBin': 'Scan Bin',
    'scanner.scanSticker': 'Scan Sticker',
    'scanner.binScanned': '✅ Bin Scanned',
    'scanner.bucketRegistered': '✅ Bucket registered!',

    // Quality Control
    'qc.inspection': 'Quality Inspection',
    'qc.grade': 'Grade',
    'qc.good': 'Good',
    'qc.warning': 'Warning',
    'qc.bad': 'Bad',
    'qc.viewHistory': 'View Inspection History',
    'qc.noInspections': 'No inspections yet',
    'qc.inspectionHistory': 'Inspection History',
    'qc.inspector': 'Inspector',
    'qc.date': 'Date',
    'qc.notes': 'Notes',

    // Team
    'team.addMember': 'Add Team Member',
    'team.assignRow': 'Assign Row',
    'team.onBreak': 'On Break',
    'team.active': 'Active',
    'team.inactive': 'Inactive',
    'team.performance': 'Performance',
    'team.bucketsToday': 'Buckets Today',
    'team.hoursWorked': 'Hours Worked',

    // Alerts
    'alert.hydration': 'Hydration Reminder',
    'alert.safety': 'Safety Alert',
    'alert.weather': 'Weather Alert',
    'alert.emergency': 'Emergency',
    'alert.acknowledge': 'Acknowledge',
    'alert.moveNow': 'CRITICAL: MOVE BIN NOW!',
    'alert.fruitDeteriorating': 'Fruit quality deteriorating',
    'alert.acknowledgeTransport': 'Acknowledge & Transport',

    // Profile
    'profile.settings': 'Settings',
    'profile.language': 'Language',
    'profile.logout': 'Log Out',
    'profile.version': 'Version',

    // Error Boundary
    'error.title': 'Something went wrong',
    'error.description': 'The application encountered an unexpected error.',
    'error.reload': 'Reload Application',
    'error.clearCache': 'Clear Cache & Reload',

    // Picker Profile
    'picker.todayPerformance': "Today's Performance",
    'picker.buckets': 'Buckets',
    'picker.speed': '/hr Speed',
    'picker.earnings': 'Earnings',
    'picker.effectiveRate': 'Effective Rate',
    'picker.belowMinimum': 'Below',
    'picker.details': 'Details',
    'picker.currentRow': 'Current Row',
    'picker.unassigned': 'Unassigned',
    'picker.harness': 'Harness',
    'picker.notAssigned': 'Not assigned',
    'picker.hoursToday': 'Hours Today',
    'picker.noTeam': 'No team',
    'picker.assigned': 'Assigned',
    'picker.rowNumber': 'Row Number',
    'picker.status': 'Status',

    // Dashboard
    'dashboard.title': 'Dashboard',
    'dashboard.totalBuckets': 'Total Buckets',
    'dashboard.activePickers': 'Active Pickers',
    'dashboard.avgRate': 'Avg Rate',
    'dashboard.compliance': 'Compliance',
};
