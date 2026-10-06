export interface MoviePreset {
  id: string;
  name: string;
  year?: string;
  badge: string;
  details: string;
  url: string;
}

export const HARDCODED_PRESETS: MoviePreset[] = [
  {
    id: 'antman',
    name: 'Ant-Man and the Wasp: Quantumania',
    year: '2023',
    badge: 'HEVC 10-bit • AAC 5.1',
    details: '1080p DSNP-WEB x265 HEVC 10bit AAC 5.1 MSubs-PSA',
    url: 'http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282023%29%201080p/Ant-Man%20and%20the%20Wasp-Quantumania%20%282023%29%201080p%20DSNP/Ant-Man%20and%20the%20Wasp%20Quantumania%20%282023%29%201080p%20DSNP-WEB%20x265%20HEVC%2010bit%20AAC%205.1%20MSubs-PSA.mkv',
  },
  {
    id: 'civil-war',
    name: 'Civil War',
    year: '2024',
    badge: 'Dual Audio: Hindi + English 5.1',
    details: '1080p BluRay x265 HEVC ESub [Hindi 5.1 + English 5.1] -mkvC',
    url: 'http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282024%29%201080p/Civil%20War%20%282024%29%201080p%20%5BDual%20Audio%5D/Civil%20War%20%282024%29%201080p%20BluRay%20x265%20HEVC%20ESub%20%5BDual%20Audio%5D%5BHindi%205.1%2BEnglish%205.1%5D%20-mkvC.mkv',
  },
  {
    id: 'kingdom-s1e1',
    name: 'Kingdom (Season 1 Episode 1)',
    year: '2019',
    badge: 'Dual Audio: English + Korean 5.1',
    details: 'S01E01 1080p NF WEBRip x265 HEVC MSubs [English 5.1 + Korean 5.1] -OlaM',
    url: 'http://172.16.50.14/DHAKA-FLIX-14/KOREAN%20TV%20%26%20WEB%20Series/Kingdom%20%28TV%20Series%202019%E2%80%93%20%29%201080p%20%5BDual%20Audio%5D/Season%201/Kingdom%20S01E01%20%201080p%20NF%20WEBRip%20x265%20HEVC%20MSubs%20%5BDual%20Audio%5D%5BEnglish%205.1%2BKorean%205.1%5D%20-OlaM.mkv',
  },
  {
    id: 'control-mp4',
    name: 'Big Buck Bunny (Control Sample)',
    year: 'MP4',
    badge: 'H.264 • AAC 2.0',
    details: 'Standard public web MP4 baseline test',
    url: 'https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4',
  },
];
