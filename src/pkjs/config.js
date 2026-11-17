// Clay settings page shown in the Pebble phone app. Values mirror src/c/settings.h:
// intervals are divisors of 60, and vibration values are the VibeType enum.
var VIBE_OPTIONS = [
  { label: 'Short', value: '0' },
  { label: 'Long', value: '1' },
  { label: 'Double', value: '2' }
];

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Time Sense'
  },
  {
    type: 'text',
    defaultValue: 'Buzzes at clock-aligned intervals (:00, :05, :10…) to help you feel ' +
      'time passing.'
  },
  {
    type: 'section',
    items: [
      {
        type: 'toggle',
        messageKey: 'Enabled',
        label: 'Buzzing',
        defaultValue: true
      },
      {
        type: 'select',
        messageKey: 'Interval',
        label: 'Buzz every',
        defaultValue: '5',
        options: [1, 2, 3, 4, 5, 6, 10, 12, 15, 20, 30, 60].map(function(min) {
          return { label: min + (min === 1 ? ' minute' : ' minutes'), value: String(min) };
        })
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Vibration'
      },
      {
        type: 'select',
        messageKey: 'Vibe',
        label: 'Interval buzz',
        defaultValue: '0',
        options: VIBE_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'HourVibe',
        label: 'On the hour',
        defaultValue: '2',
        options: VIBE_OPTIONS
      }
    ]
  },
  {
    type: 'text',
    defaultValue: 'Tip: assign Time Sense to a button in the watch\'s Settings → Quick Launch ' +
      'to switch buzzing on and off with one press.'
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
