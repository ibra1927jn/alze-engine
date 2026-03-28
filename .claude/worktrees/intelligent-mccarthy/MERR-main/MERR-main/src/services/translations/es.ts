// =============================================
// SPANISH TRANSLATIONS (es)
// =============================================

export const translations: Record<string, string> = {
    // Common
    'common.loading': 'Cargando...',
    'common.save': 'Guardar',
    'common.cancel': 'Cancelar',
    'common.close': 'Cerrar',
    'common.confirm': 'Confirmar',
    'common.delete': 'Eliminar',
    'common.edit': 'Editar',
    'common.add': 'Agregar',
    'common.search': 'Buscar',
    'common.filter': 'Filtrar',
    'common.refresh': 'Actualizar',
    'common.back': 'Volver',
    'common.next': 'Siguiente',
    'common.done': 'Listo',
    'common.yes': 'Sí',
    'common.no': 'No',
    'common.ok': 'OK',
    'common.error': 'Error',
    'common.success': 'Éxito',
    'common.warning': 'Advertencia',

    // Navigation
    'nav.logistics': 'Logística',
    'nav.runners': 'Corredores',
    'nav.warehouse': 'Bodega',
    'nav.messaging': 'Mensajes',
    'nav.team': 'Equipo',
    'nav.rows': 'Hileras',
    'nav.quality': 'Calidad',
    'nav.settings': 'Ajustes',

    // Headers
    'header.logisticsHub': 'Centro de Logística',
    'header.orchardRunners': 'Corredores del Huerto',
    'header.warehouseInventory': 'Inventario de Bodega',
    'header.messagingHub': 'Centro de Mensajes',
    'header.teamManagement': 'Gestión del Equipo',
    'header.rowAssignments': 'Asignación de Hileras',

    // Offline Banner
    'offline.syncPending': 'Sincronización Pendiente',
    'offline.updated': 'Actualizado hace {{time}}',

    // Logistics View
    'logistics.bucketsCollected': 'Cubetas Recolectadas',
    'logistics.full': 'Lleno',
    'logistics.binFull': 'Bin Lleno',
    'logistics.active': 'Activo',
    'logistics.ready': 'Listo',
    'logistics.approachingLimit': '⚠️ Acercándose al límite de 72 cubetas',
    'logistics.prepareSwap': 'Preparar cambio de bin',
    'logistics.limitReached': '🚫 LÍMITE ALCANZADO - NO AGREGAR MÁS',
    'logistics.closeImmediately': 'Cerrar bin inmediatamente para prevenir daño a la fruta',

    // Sun Exposure
    'sun.exposure': 'Exposición Solar',
    'sun.critical': '🚨 ¡CRÍTICO!',
    'sun.safeLevel': 'Nivel Seguro',
    'sun.moveToShade': '¡Mover a la sombra!',

    // Supply Management
    'supply.management': 'Gestión de Suministros',
    'supply.emptyBins': 'Bins Vacíos',
    'supply.fullBins': 'Bins Llenos',
    'supply.low': '⚠️ Bajo',
    'supply.ok': 'OK',
    'supply.requestRefill': 'Solicitar Reposición',
    'supply.refillRequested': '📦 ¡Reposición solicitada!',
    'supply.binsEnRoute': '✅ {{count}} bins vacíos en camino',
    'supply.eta': '🚛 Llegada: {{minutes}} minutos desde depósito',

    // Runners
    'runners.active': 'Corredores Activos',
    'runners.addRunner': 'Agregar Corredor',
    'runners.noActive': 'Sin Corredores Activos',
    'runners.addFirst': 'Agregar Primer Corredor',
    'runners.addToTrack': 'Agrega corredores para seguir su actividad',
    'runners.manageRunner': 'Gestionar Corredor',
    'runners.started': 'Inició {{time}}',
    'runners.assignment': 'Asignación',
    'runners.noAssignment': 'Sin asignación',
    'runners.buckets': 'Cubetas',
    'runners.bins': 'Bins',
    'runners.orchardMap': 'Mapa del Huerto',
    'runners.gpsComingSoon': 'GPS en tiempo real próximamente',

    // Warehouse
    'warehouse.harvestedStock': 'Stock Cosechado',
    'warehouse.fullCherryBins': 'Bins de Cereza Llenos',
    'warehouse.filled': 'llenos',
    'warehouse.manualAdjustment': 'Ajuste Manual:',
    'warehouse.emptyBinsAvailable': 'Bins Vacíos Disponibles',
    'warehouse.waitingTransport': 'Esperando Transporte',
    'warehouse.critical': '🚨 CRÍTICO: ¡Bins vacíos agotados!',
    'warehouse.lowStock': '⚠️ Alerta de stock bajo',
    'warehouse.requestResupply': 'Solicitar reabastecimiento inmediato desde depósito',
    'warehouse.nextTruck': 'Próximo Camión de Reabastecimiento',
    'warehouse.scheduledArrival': 'Llegada programada en {{minutes}} mins desde Depósito A',
    'warehouse.requestTransport': 'Solicitar Transporte',

    // Scanner
    'scanner.scanBin': 'Escanear Bin',
    'scanner.scanSticker': 'Escanear Sticker',
    'scanner.binScanned': '✅ Bin Escaneado',
    'scanner.bucketRegistered': '✅ ¡Cubeta registrada!',

    // Quality Control
    'qc.inspection': 'Inspección de Calidad',
    'qc.grade': 'Grado',
    'qc.good': 'Bueno',
    'qc.warning': 'Advertencia',
    'qc.bad': 'Malo',
    'qc.viewHistory': 'Ver Historial de Inspecciones',
    'qc.noInspections': 'Sin inspecciones aún',
    'qc.inspectionHistory': 'Historial de Inspecciones',
    'qc.inspector': 'Inspector',
    'qc.date': 'Fecha',
    'qc.notes': 'Notas',

    // Team
    'team.addMember': 'Agregar Miembro',
    'team.assignRow': 'Asignar Hilera',
    'team.onBreak': 'En Descanso',
    'team.active': 'Activo',
    'team.inactive': 'Inactivo',
    'team.performance': 'Desempeño',
    'team.bucketsToday': 'Cubetas Hoy',
    'team.hoursWorked': 'Horas Trabajadas',

    // Alerts
    'alert.hydration': 'Recordatorio de Hidratación',
    'alert.safety': 'Alerta de Seguridad',
    'alert.weather': 'Alerta del Clima',
    'alert.emergency': 'Emergencia',
    'alert.acknowledge': 'Confirmar',
    'alert.moveNow': 'CRÍTICO: ¡MOVER BIN AHORA!',
    'alert.fruitDeteriorating': 'Calidad de fruta deteriorándose',
    'alert.acknowledgeTransport': 'Confirmar y Transportar',

    // Profile
    'profile.settings': 'Ajustes',
    'profile.language': 'Idioma',
    'profile.logout': 'Cerrar Sesión',
    'profile.version': 'Versión',

    // Error Boundary
    'error.title': 'Algo salió mal',
    'error.description': 'La aplicación encontró un error inesperado.',
    'error.reload': 'Recargar Aplicación',
    'error.clearCache': 'Limpiar Caché y Recargar',

    // Picker Profile
    'picker.todayPerformance': 'Rendimiento de Hoy',
    'picker.buckets': 'Cubetas',
    'picker.speed': '/hr Velocidad',
    'picker.earnings': 'Ganancias',
    'picker.effectiveRate': 'Tarifa Efectiva',
    'picker.belowMinimum': 'Debajo',
    'picker.details': 'Detalles',
    'picker.currentRow': 'Fila Actual',
    'picker.unassigned': 'Sin asignar',
    'picker.harness': 'Arnés',
    'picker.notAssigned': 'No asignado',
    'picker.hoursToday': 'Horas Hoy',
    'picker.noTeam': 'Sin equipo',
    'picker.assigned': 'Asignado',
    'picker.rowNumber': 'Número de Fila',
    'picker.status': 'Estado',

    // Dashboard
    'dashboard.title': 'Panel',
    'dashboard.totalBuckets': 'Total Cubetas',
    'dashboard.activePickers': 'Cosechers Activos',
    'dashboard.avgRate': 'Tarifa Promedio',
    'dashboard.compliance': 'Cumplimiento',
};
