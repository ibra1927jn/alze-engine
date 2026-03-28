/**
 * Generate PNG icons from SVG for PWA compatibility
 * Run: node scripts/generate-icons.mjs
 */
import sharp from 'sharp';
import { readFileSync, writeFileSync, existsSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const publicDir = join(__dirname, '..', 'public');
const iconsDir = join(publicDir, 'icons');
const svgPath = join(iconsDir, 'icon.svg');

const svgBuffer = readFileSync(svgPath);

const sizes = [
    { name: 'icon-192x192.png', size: 192 },
    { name: 'icon-512x512.png', size: 512 },
    { name: 'apple-touch-icon.png', size: 180 },
    { name: 'favicon-32x32.png', size: 32 },
    { name: 'favicon-16x16.png', size: 16 },
];

for (const { name, size } of sizes) {
    const isAppleOrFavicon = name.startsWith('apple') || name.startsWith('favicon');
    const outPath = join(isAppleOrFavicon ? publicDir : iconsDir, name);

    await sharp(svgBuffer)
        .resize(size, size)
        .png()
        .toFile(outPath);

    console.log(`✅ Generated ${name} (${size}x${size})`);
}

// Generate favicon.ico (use 32x32 PNG as ICO placeholder)
const favicon32 = join(publicDir, 'favicon-32x32.png');
if (existsSync(favicon32)) {
    // Copy 32x32 as favicon.ico (browsers accept PNG as .ico)
    const pngData = readFileSync(favicon32);
    writeFileSync(join(publicDir, 'favicon.ico'), pngData);
    console.log('✅ Generated favicon.ico (from 32x32 PNG)');
}

console.log('\\n🎉 All PWA icons generated successfully!');
