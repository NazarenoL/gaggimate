import { useEffect, useState } from 'preact/hooks';
import { machine } from '../../services/ApiService.js';
import { useBeans, validGrindSize } from '../../services/beans.js';

export function BeanPicker() {
  const { beans, selectedId, lastGrindSetting, request, disabled, error } = useBeans();
  const bean = beans.find(item => item.id === selectedId);
  const savedGrind = bean?.grindSetting == null ? '' : Number(bean.grindSetting).toFixed(1);
  const prefilledGrind =
    bean && lastGrindSetting != null ? Number(lastGrindSetting).toFixed(1) : '';
  const initialGrind = savedGrind || prefilledGrind;
  const [draft, setDraft] = useState(null);
  const grind = draft?.id === selectedId ? draft.value : initialGrind;
  const active = !!machine.value.status.process?.a;
  const changed = grind !== savedGrind;
  const valid = validGrindSize(grind);

  useEffect(() => setDraft(null), [selectedId, initialGrind]);

  return (
    <div className='card bg-base-100 space-y-2 rounded-xl p-3'>
      <div className='flex items-center justify-between'>
        <h3 className='font-semibold'>Beans &amp; grind size</h3>
        <a href='/beans' className='link text-sm'>
          Manage beans
        </a>
      </div>
      <label className='flex flex-col gap-1 text-sm'>
        Beans
        <select
          aria-label='Beans'
          className='select w-full'
          value={selectedId}
          disabled={disabled || active}
          onChange={event => request('select', { id: event.target.value })}
        >
          <option value=''>No beans selected</option>
          {beans.map(item => (
            <option key={item.id} value={item.id}>
              {item.roaster} — {item.name}
            </option>
          ))}
        </select>
      </label>
      <form
        className='flex items-end gap-2'
        onSubmit={event => {
          event.preventDefault();
          if (valid) request('select', { id: selectedId, grindSetting: Number(grind) });
        }}
      >
        <label className='flex flex-1 flex-col gap-1 text-sm'>
          Grind size (1–16)
          <input
            className='input w-full'
            type='number'
            min='1'
            max='16'
            step='0.1'
            required
            value={grind}
            placeholder='6.1'
            disabled={disabled || active || !bean}
            onInput={event => setDraft({ id: selectedId, value: event.target.value })}
          />
        </label>
        <button
          className='btn'
          type='submit'
          disabled={disabled || active || !bean || !changed || !valid}
        >
          Save
        </button>
      </form>
      {changed && (
        <p className='text-warning text-xs'>
          {valid
            ? 'Save your grind size before brewing.'
            : 'Enter a grind size from 1 to 16 in steps of 0.1.'}
        </p>
      )}
      {error && (
        <p role='alert' className='text-error text-sm'>
          {error}
        </p>
      )}
    </div>
  );
}
