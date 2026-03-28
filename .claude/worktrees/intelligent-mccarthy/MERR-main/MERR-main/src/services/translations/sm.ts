// =============================================
// SAMOAN TRANSLATIONS (sm)
// =============================================

export const translations: Record<string, string> = {
    // Common
    'common.loading': 'O loʻo faʻapipiʻi...',
    'common.save': 'Sefe',
    'common.cancel': 'Faʻaleaoga',
    'common.close': 'Tapuni',
    'common.confirm': 'Faʻamaonia',
    'common.delete': 'Tape',
    'common.edit': 'Faʻasaʻo',
    'common.add': 'Faʻaopoopo',
    'common.search': 'Suʻe',
    'common.filter': 'Filiga',
    'common.refresh': 'Faʻafouina',
    'common.back': 'Toe foi',
    'common.next': 'Isi',
    'common.done': 'Ua maeʻa',
    'common.yes': 'Ioe',
    'common.no': 'Leai',
    'common.ok': 'Ua lelei',
    'common.error': 'Mea sese',
    'common.success': 'Manuia',
    'common.warning': 'Lapataiga',

    // Navigation
    'nav.logistics': 'Faʻasoa',
    'nav.runners': 'Tagata tamo',
    'nav.warehouse': 'Faleoloa',
    'nav.messaging': 'Fesoʻotaʻiga',
    'nav.team': 'Au',
    'nav.rows': 'Laina',
    'nav.quality': 'Lelei',
    'nav.settings': 'Faʻatulagaga',

    // Headers
    'header.logisticsHub': 'Nofoaga Faʻasoa',
    'header.orchardRunners': 'Tagata Tamo o le Togālaau',
    'header.warehouseInventory': 'Mea i le Faleoloa',
    'header.messagingHub': 'Nofoaga Fesoʻotaʻi',
    'header.teamManagement': 'Puleaga o le Au',
    'header.rowAssignments': 'Tofiaga o Laina',

    // Offline Banner
    'offline.syncPending': 'O Faʻatali le Tuʻufaʻatasia',
    'offline.updated': 'Faʻafouina {{time}} talu ai',

    // Logistics View
    'logistics.bucketsCollected': 'Pakete Ua Aoina',
    'logistics.full': 'Tumu',
    'logistics.binFull': 'Pusa Tumu',
    'logistics.active': 'Galue',
    'logistics.ready': 'Sauni',
    'logistics.approachingLimit': '⚠️ O latalata i le 72 pakete',
    'logistics.prepareSwap': 'Sauni e sui le pusa',
    'logistics.limitReached': '🚫 UA OʻO I LE TAPULAʻA - AUA LE TOE FAʻAOPOOPO',
    'logistics.closeImmediately': 'Tapuni vave le pusa e puipuia fualaau',

    // Sun Exposure
    'sun.exposure': 'Susulu o le La',
    'sun.critical': '🚨 TAUTEʻI!',
    'sun.safeLevel': 'Tulaga Saogalemu',
    'sun.moveToShade': 'Aveina i le paolo!',

    // Supply Management
    'supply.management': 'Puleaga o Mea',
    'supply.emptyBins': 'Pusa Avanoa',
    'supply.fullBins': 'Pusa Tumu',
    'supply.low': '⚠️ Maualalo',
    'supply.ok': 'Ua lelei',
    'supply.requestRefill': 'Talosaga mo Mea',
    'supply.refillRequested': '📦 Ua talosagaina mea!',
    'supply.binsEnRoute': '✅ {{count}} pusa avanoa o loʻo sau',
    'supply.eta': '🚛 Taimi taunuʻu: {{minutes}} minute mai le fale',

    // Runners
    'runners.active': 'Tagata Tamo Galue',
    'runners.addRunner': 'Faʻaopopo Tamo',
    'runners.noActive': 'Leai se Tamo Galue',
    'runners.addFirst': 'Faʻaopopo Muamua Tamo',
    'runners.addToTrack': 'Faʻaopopo tagata tamo e siaki',
    'runners.manageRunner': 'Puleaina le Tamo',
    'runners.started': 'Amata {{time}}',
    'runners.assignment': 'Tofiaga',
    'runners.noAssignment': 'Leai se tofiaga',
    'runners.buckets': 'Pakete',
    'runners.bins': 'Pusa',
    'runners.orchardMap': 'Faafanua Togālaau',
    'runners.gpsComingSoon': 'GPS i taimi moni e sau',

    // Warehouse
    'warehouse.harvestedStock': 'Mea Ua Seleseleina',
    'warehouse.fullCherryBins': 'Pusa Tipolo Tumu',
    'warehouse.filled': 'tumu',
    'warehouse.manualAdjustment': 'Suiga Lima:',
    'warehouse.emptyBinsAvailable': 'Pusa Avanoa',
    'warehouse.waitingTransport': 'O Faʻatali le Feaveaʻi',
    'warehouse.critical': '🚨 TAUTEʻI: Ua uma pusa avanoa!',
    'warehouse.lowStock': '⚠️ Lapataiga mea maualalo',
    'warehouse.requestResupply': 'Talosaga vave mea mai le fale',
    'warehouse.nextTruck': 'Loli o le a sau',
    'warehouse.scheduledArrival': 'E taunuʻu i {{minutes}} minute mai le Fale A',
    'warehouse.requestTransport': 'Talosaga Feaveaʻi',

    // Scanner
    'scanner.scanBin': 'Siaki Pusa',
    'scanner.scanSticker': 'Siaki Pepelo',
    'scanner.binScanned': '✅ Ua siakiina le Pusa',
    'scanner.bucketRegistered': '✅ Ua resitala le pakete!',

    // Quality Control
    'qc.inspection': 'Siakiga Lelei',
    'qc.grade': 'Tulaga',
    'qc.good': 'Lelei',
    'qc.warning': 'Lapataiga',
    'qc.bad': 'Leaga',
    'qc.viewHistory': 'Vaʻai Tala o Siakiga',
    'qc.noInspections': 'Leai ni siakiga',
    'qc.inspectionHistory': 'Tala o Siakiga',
    'qc.inspector': 'Tagata Siaki',
    'qc.date': 'Aso',
    'qc.notes': 'Tusitusiga',

    // Team
    'team.addMember': 'Faʻaopopo Tagata',
    'team.assignRow': 'Tofia Laina',
    'team.onBreak': 'O loʻo Malolo',
    'team.active': 'Galue',
    'team.inactive': 'Le galue',
    'team.performance': 'Faatinoga',
    'team.bucketsToday': 'Pakete i Aso nei',
    'team.hoursWorked': 'Itula na Galue',

    // Alerts
    'alert.hydration': 'Faʻamanatu e Inu',
    'alert.safety': 'Lapataiga Saogalemu',
    'alert.weather': 'Lapataiga Tau',
    'alert.emergency': 'Faʻalavelave',
    'alert.acknowledge': 'Talia',
    'alert.moveNow': 'TAUTEʻI: AVE LE PUSA NEI!',
    'alert.fruitDeteriorating': 'O loʻo leaga fualaau',
    'alert.acknowledgeTransport': 'Talia ma Aveina',

    // Profile
    'profile.settings': 'Faʻatulagaga',
    'profile.language': 'Gagana',
    'profile.logout': 'Alu ese',
    'profile.version': 'Lomiga',

    // Error Boundary
    'error.title': 'Ua i ai se mea sese',
    'error.description': 'Ua maua e le polokalame se mea sese.',
    'error.reload': 'Toe Faʻapipiʻi',
    'error.clearCache': 'Faʻamamā ma Toe Faʻapipiʻi',

    // Picker Profile
    'picker.todayPerformance': 'Faatinoga i Aso nei',
    'picker.buckets': 'Pakete',
    'picker.speed': '/itu Vave',
    'picker.earnings': 'Tupe Maua',
    'picker.effectiveRate': 'Fua Moni',
    'picker.belowMinimum': 'I Lalo',
    'picker.details': 'Faʻamatalaga',
    'picker.currentRow': 'Laina nei',
    'picker.unassigned': 'Le tofia',
    'picker.harness': 'Mea Nofo',
    'picker.notAssigned': 'Le tofia',
    'picker.hoursToday': 'Itula i Aso nei',
    'picker.noTeam': 'Leai se au',
    'picker.assigned': 'Tofia',
    'picker.rowNumber': 'Numera Laina',
    'picker.status': 'Tulaga',

    // Dashboard
    'dashboard.title': 'Vaega Autu',
    'dashboard.totalBuckets': 'Aofaʻi Pakete',
    'dashboard.activePickers': 'Tagata Selesele Galue',
    'dashboard.avgRate': 'Averesi Fua',
    'dashboard.compliance': 'Usitaʻi',
};
